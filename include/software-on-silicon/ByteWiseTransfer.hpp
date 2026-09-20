namespace SOS {
namespace Protocol {
    template<typename... Objects>
    class SyncProcessor {
    public:
        SyncProcessor(SOS::MemoryView::SerialResolverBus<Objects...>& bus2);
    protected:
        bool check_sync(std::size_t obj_id) {
            if (!_sBus.signal[obj_id].sync_me.test_and_set()) {
                _sBus.signal[obj_id].sync_me.clear();
                return true;
            }
            return false;
        }
        void emit_init() {}
        void emit_interrupted()
        {
            for (std::size_t i = 0; i < _sBus.signal.size(); ++i) {
                if (read_started_id[i]) {
                    _sBus.signal[i].read_op.getNotifyRef().test_and_set();
                    std::cout << typeid(*this).name() << ": object id " << i << " enters inaccessible state" << std::endl;
                    _sBus.signal[i].read_fault.clear();
                    read_started_id[i] = false;
                }
            }
        }
        void emit_readlocked(std::size_t obj_id)
        {
            //SFA::util::logic_error(SFA::util::error_code::ObjectSyncWasNeverRequested, __FILE__, __func__, typeid(*this).name());
            _sBus.signal[obj_id].read_op.getNotifyRef().clear();
            if (!_sBus.signal[obj_id].sync_me.test_and_set()) {
                _sBus.signal[obj_id].write_op.getNotifyRef().test_and_set();
                _sBus.signal[obj_id].write_fault.clear();
                _sBus.signal[obj_id].write_ack.clear();
            }
            read_started_id[obj_id] = true;
        }
        void emit_transfer(std::size_t obj_id)
        {
            _sBus.signal[obj_id].write_op.getNotifyRef().clear();
            _sBus.signal[obj_id].sync_me.test_and_set();
        }
        void emit_received(std::size_t obj_id)
        {
            _sBus.signal[obj_id].read_op.getNotifyRef().test_and_set();
            read_started_id[obj_id] = false;
            _sBus.signal[obj_id].read_ack.clear();
        }
        void emit_sent(std::size_t obj_id)
        {
            if (obj_id == 1 || obj_id == 2) {
                std::cout << typeid(*this).name() << ": write of object id " << obj_id << " succeeded" << std::endl;
            }
            _sBus.signal[obj_id].write_op.getNotifyRef().test_and_set();
            _sBus.signal[obj_id].write_ack.clear();
        }
        void trigger_resolve() { _sBus.signal.getNotifyRef().clear(); }
    protected:
        std::tuple<Objects...> objects {};
        SOS::Protocol::DescriptorHelper descriptors;
    private:
        SOS::MemoryView::SerialResolverBus<Objects...>& _sBus;
        std::bitset<NUM_IDS> read_started_id {};
    };
    template <typename... Objects>
    class BlockWiseTransfer : protected SyncProcessor<Objects...> { // write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
    public:
        using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
        BlockWiseTransfer(bus_type& bus, SOS::MemoryView::SerialResolverBus<Objects...>& bus2)
        : SyncProcessor<Objects...>(bus2)
        , _com(bus) {}

    protected:
        bool write_object()
        {
            if (send_lock) {
                if (write3plus1 < 3) {
                    unsigned char data;
                    //bus2.signal[writeOrigin].write_op.getSecondRef().clear();
                    //bus2.signal[writeOrigin].write_op.getFirstRef().clear();
                    data = reinterpret_cast<char*>(this->descriptors[writeOrigin].obj)[writeOriginPos++];
                    //bus2.signal[writeOrigin].write_op.getFirstRef().test_and_set();
                    //bus2.signal[writeOrigin].write_op.getSecondRef().test_and_set();
                    write3plus1++;
                    write(data);
                    return true;
                } else { // write3plus1==3
                    if (writeOriginPos == this->descriptors[writeOrigin].obj_size) {
                        this->emit_sent(writeOrigin);
                        this->descriptors[writeOrigin].transfer = false;
                        send_lock = false;
                        ++tx_counter[writeOrigin]; // DEBUG
                        // std::cout << typeid(*this).name() << ":" << "W" << std::to_string(writeOrigin) << std::endl;
                        writeOriginPos = 0;
                    }
                    write3plus1 = 0;
                    write(63); //'?' empty write
                    return true;
                }
            }
            return false;
        }
        void read_object(unsigned char& data)
        {
            if (!receive_lock) {
                bool gotOne = false;
                for (unsigned char j = 0; j < this->descriptors.size() && !gotOne; j++) {
                    if (this->descriptors[j].readLock) {
                        if (readDestinationPos != 0) {
                            SFA::util::logic_error(SFA::util::error_code::PreviousReadobjectHasNotFinished, __FILE__, __func__, typeid(*this).name());
                        }
                        receive_lock = true;
                        readDestination = j;
                        gotOne = true;
                    }
                }
            }
            if (receive_lock) {
                read(data);
                if (read4minus1 < 3) {
                    read4minus1++;
                } else if (read4minus1 == 3) {
                    auto read3bytes = read_flush();
                    if (readDestinationPos < this->descriptors[readDestination].obj_size) {
                        //bus2.signal[readDestination].read_op.getSecondRef().clear();
                        //bus2.signal[readDestination].read_op.getFirstRef().clear();
                        for (std::size_t i = 0; i < 3; i++) {
                            reinterpret_cast<char*>(this->descriptors[readDestination].obj)[readDestinationPos++] = read3bytes[i];
                        }
                        //bus2.signal[readDestination].read_op.getFirstRef().test_and_set();
                        //bus2.signal[readDestination].read_op.getSecondRef().test_and_set();
                    }
                    if (readDestinationPos == this->descriptors[readDestination].obj_size) {
                        this->descriptors[readDestination].readLock = false;
                        receive_lock = false;
                        this->emit_received(readDestination);
                        ++rx_counter[readDestination]; // DEBUG
                        // std::cout << typeid(*this).name() << "." << "R" << std::to_string(readDestination) << std::endl;
                        readDestinationPos = 0;
                    }
                    read4minus1 = 0;
                }
            } else {
                SFA::util::logic_error(SFA::util::error_code::NoIdleReceivedAndNoReceivelockObtained, __FILE__, __func__, typeid(*this).name());
            }
        }
        unsigned char read_byte()
        {
            const auto bufferLength = std::distance(std::get<0>(_com.const_cables).getInBufferStartRef(), std::get<0>(_com.const_cables).getInBufferStartRef());
            if (std::get<0>(_com.cables).getReadOffsetRef().load() > bufferLength)
                SFA::util::runtime_error(SFA::util::error_code::AttemptedReadAfterEndOfBuffer, __FILE__, __func__, typeid(*this).name());
            auto next = std::get<0>(_com.cables).getReadOffsetRef().load();
            auto byte = *(std::get<0>(_com.const_cables).getInBufferStartRef() + next);
            next++;
            if (next >= bufferLength)
                std::get<0>(_com.cables).getReadOffsetRef().store(0);
            else
                std::get<0>(_com.cables).getReadOffsetRef().store(next);
            return byte;
        }
        void write_byte(unsigned char byte)
        {
            const auto bufferLength = std::distance(std::get<0>(_com.const_cables).getOutBufferStartRef(), std::get<0>(_com.const_cables).getOutBufferEndRef());
            if (std::get<0>(_com.cables).getWriteOffsetRef().load() > bufferLength)
                SFA::util::runtime_error(SFA::util::error_code::AttemptedWriteAfterEndOfBuffer, __FILE__, __func__, typeid(*this).name());
            auto next = std::get<0>(_com.cables).getWriteOffsetRef().load();
            *(std::get<0>(_com.const_cables).getOutBufferStartRef() + next) = byte;
            next++;
            if (next >= bufferLength)
                std::get<0>(_com.cables).getWriteOffsetRef().store(0);
            else
                std::get<0>(_com.cables).getWriteOffsetRef().store(next);
        }
        virtual void read_bits(std::bitset<8> temp) = 0;
        virtual void write_bits(std::bitset<8>& out) = 0;
        bool receive_lock = false;
        std::size_t readDestinationPos = 0;
        unsigned int readCount = 0; // read4minus1
        unsigned char readDestination = NUM_IDS;
        bool send_lock = false;
        std::size_t writeOriginPos = 0;
        unsigned int writeCount = 0; // write3plus1
        unsigned char writeOrigin = NUM_IDS;
        //virtual void emit_received(std::size_t obj_id) = 0;
        //virtual void emit_sent(std::size_t obj_id) = 0;
        //SOS::MemoryView::SerialResolverBus<Objects...> bus2;
        //SOS::MemoryView::SequentialBus bus3 {};
        SOS::MemoryView::ComBus<COM_BUFFER>& _com;
        std::array<unsigned long, NUM_IDS> rx_counter { 0 }; // DEBUG
        std::array<unsigned long, NUM_IDS> tx_counter { 0 }; // DEBUG

    private:
        std::array<std::bitset<8>, 3> writeAssembly;
        std::bitset<24> readAssembly;
        void write(unsigned char w)
        {
            std::bitset<8> out;
            switch (writeCount) {
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
            std::bitset<24> temp { static_cast<unsigned long>(r) };
            read_shift(readAssembly, readCount, temp);
            switch (readCount) {
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
        static std::bitset<8> write_assemble(decltype(writeAssembly)& writeAssembly, decltype(writeCount)& writeCount, unsigned char w)
        {
            std::bitset<8> out;
            writeAssembly[writeCount] = w;
            out = writeAssembly[writeCount] >> (writeCount + 1) * 2;
            return out;
        }
        static std::bitset<8> write_recover(decltype(writeAssembly)& writeAssembly, decltype(writeCount)& writeCount, std::bitset<8>& out)
        {
            std::bitset<8> cache;
            cache = writeAssembly[writeCount - 1] << (4 - writeCount) * 2;
            cache = cache >> 1 * 2;
            return out ^ cache;
        }
        static void read_shift(decltype(readAssembly)& readAssembly, decltype(readCount)& readCount, std::bitset<24>& temp)
        {
            temp = temp << (4 - 0) * 4 + 2; // split off 1st 2bit
            temp = temp >> (readCount * 3) * 2; // shift
            readAssembly = readAssembly ^ temp; // overlay
        }
        int read4minus1 = 0;
        int write3plus1 = 0;
    };
}
}