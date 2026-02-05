namespace SOS {
    namespace Protocol {
        template<typename... Objects> class BlockWiseTransfer {// write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
        public:
            BlockWiseTransfer(){}
        protected:
            bool write_object()
            {
                if (send_lock) {
                if (write3plus1 < 3)
                {
                    unsigned char data;
                    data = reinterpret_cast<char *>(foreign().descriptors[writeOrigin].obj)[writeOriginPos++];
                    write3plus1++;
                    write(data);
                    return true;
                }
                else
                { // write3plus1==3
                    if (writeOriginPos == foreign().descriptors[writeOrigin].obj_size)
                    {
                        foreign().descriptors[writeOrigin].transfer = false;
                        foreign().descriptors[writeOrigin].synced = true;
                        send_lock = false;
                        foreign().sendNotificationId().store(writeOrigin);
                        foreign().signal.getSecondRef().clear();
                        foreign().descriptors[writeOrigin].tx_counter++; // DEBUG
                        std::cout<<typeid(*this).name()<<":"<<"W"<<std::to_string(writeOrigin)<<std::endl;
                        writeOriginPos = 0;
                    }
                    write3plus1 = 0;
                    write(63); //'?' empty write
                    return true;
                }
                }
                return false;
            }
            void read_object(unsigned char &data)
            {
                if (!receive_lock){
                    bool gotOne = false;
                    for (unsigned char j = 0; j < foreign().descriptors.size() && !gotOne; j++){
                        foreign().descriptors[j].queued = false;
                        if (foreign().descriptors[j].readLock){
                            if (readDestinationPos != 0){
                                SFA::util::logic_error(SFA::util::error_code::PreviousReadobjectHasNotFinished,__FILE__,__func__, typeid(*this).name());
                            }
                            receive_lock = true;
                            readDestination = j;
                            gotOne = true;
                        }
                    }
                    //Never reached because of idle
                    //if (!gotOne)
                    //    if (!received_writes_finished)
                    //        received_writes_finished = true;
                }
                if (receive_lock){
                read(data);
                if (read4minus1 < 3)
                {
                    read4minus1++;
                }
                else if (read4minus1 == 3)
                {
                    auto read3bytes = read_flush();
                    if (readDestinationPos < foreign().descriptors[readDestination].obj_size)
                    {
                        for (std::size_t i = 0; i < 3; i++)
                        {
                            reinterpret_cast<char *>(foreign().descriptors[readDestination].obj)[readDestinationPos++] = read3bytes[i];
                        }
                    }
                    if (readDestinationPos == foreign().descriptors[readDestination].obj_size)
                    {
                        foreign().descriptors[readDestination].readLock = false;
                        receive_lock = false;
                        foreign().receiveNotificationId().store(readDestination);
                        foreign().signal.getFirstRef().clear();
                        foreign().descriptors[readDestination].rx_counter++; // DEBUG
                        std::cout<<typeid(*this).name()<<"."<<"R"<<std::to_string(readDestination)<<std::endl;
                    readDestinationPos = 0;
                    }
                read4minus1 = 0;
                }
                } else {
                    SFA::util::logic_error(SFA::util::error_code::NoIdleReceivedAndNoReceivelockObtained,__FILE__,__func__, typeid(*this).name());
                }
            }
            virtual unsigned char read_byte() = 0;
            virtual void read_bits(std::bitset<8> temp) = 0;
            virtual void write_byte(unsigned char) = 0;
            virtual void write_bits(std::bitset<8> &out) = 0;
            virtual constexpr typename SOS::MemoryView::SerialProcessNotifier<Objects...> &foreign() = 0;
            bool receive_lock = false;
            std::size_t readDestinationPos = 0;
            unsigned int readCount = 0; // read4minus1
            unsigned char readDestination = NUM_IDS;
            bool send_lock = false;
            std::size_t writeOriginPos = 0;
            unsigned int writeCount = 0; // write3plus1
            unsigned char writeOrigin = NUM_IDS;
        private:
            std::array<std::bitset<8>, 3> writeAssembly;
            std::bitset<24> readAssembly;
            void write(unsigned char w)
            {
                std::bitset<8> out;
                switch (writeCount)
                {
                case 0:
                    out = write_assemble(writeAssembly, writeCount, w);
                    write_bits(out);
                    writeCount++;
                    break;
                case 1: // recover last 1 2bit
                case 2: // recover last 2 2bit; call 3
                    out = write_assemble(writeAssembly, writeCount, w);
                    write_bits(out);
                    out = write_recover(writeAssembly, writeCount, out);
                    writeCount++;
                    break;
                case 3: // recover 3 2bit from call 3 only
                    write_bits(out);
                    out = write_recover(writeAssembly, writeCount, out);
                    writeCount = 0;
                    break;
                }
                write_byte(static_cast<unsigned char>(out.to_ulong()));
            }
            bool read(unsigned char r)
            {
                std::bitset<24> temp{static_cast<unsigned long>(r)};
                read_shift(readAssembly, readCount, temp);
                switch (readCount)
                {
                    case 0:
                    case 1:
                    case 2:
                        readCount++;
                        return true;
                    case 3:
                        readCount = 0;
                }
                return false;
            }
            std::array<unsigned char, 3> read_flush()
            {
                std::array<unsigned char, 3> result;
                bitsetToBytearray(result, readAssembly);
                readAssembly.reset();
                return result;
            }
            static std::bitset<8> write_assemble(decltype(writeAssembly) &writeAssembly, decltype(writeCount) &writeCount, unsigned char w)
            {
                std::bitset<8> out;
                writeAssembly[writeCount] = w;
                out = writeAssembly[writeCount] >> (writeCount + 1) * 2;
                return out;
            }
            static std::bitset<8> write_recover(decltype(writeAssembly) &writeAssembly, decltype(writeCount) &writeCount, std::bitset<8> &out)
            {
                std::bitset<8> cache;
                cache = writeAssembly[writeCount - 1] << (4 - writeCount) * 2;
                cache = cache >> 1 * 2;
                return out ^ cache;
            }
            static void read_shift(decltype(readAssembly) &readAssembly, decltype(readCount) &readCount, std::bitset<24> &temp)
            {
                temp = temp << (4 - 0) * 4 + 2;     // split off 1st 2bit
                temp = temp >> (readCount * 3) * 2; // shift
                readAssembly = readAssembly ^ temp; // overlay
            }
            int read4minus1 = 0;
            int write3plus1 = 0;
        };
    }
}