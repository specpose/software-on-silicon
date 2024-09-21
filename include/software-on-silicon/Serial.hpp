namespace SOS
{
    namespace Protocol
    {
        static std::bitset<8> idleState()
        { // constexpr
            std::bitset<8> id;
            for (std::size_t i = 0; i < id.size(); i++)
            {
                id.set(i, 1);
            }
            id.set(7, 1); // updated==true
            id.set(6, 0); // acknowledge==false
            return id;    //-> "10111111"
        }
        static std::bitset<8> shutdownState()
        { // constexpr
            std::bitset<8> id;
            for (std::size_t i = 0; i < id.size(); i++)
            {
                id.set(i, 1);
            }
            id.set(7, 1); // updated==true
            id.set(6, 0); // acknowledge==false
            id.set(0, 0);
            return id; //-> "10111110"
        }
        // EMULATION: long sync times and instant poweroff
        static std::bitset<8> poweronState()
        { // constexpr
            std::bitset<8> id;
            for (std::size_t i = 0; i < id.size(); i++)
            {
                id.set(i, 1);
            }
            id.set(7, 1); // updated==true
            id.set(6, 0); // acknowledge==false
            id.set(1, 0);
            id.set(0, 0);
            return id; //-> "10111100"
        }
        struct DMADescriptor
        {
            DMADescriptor() {} // DANGER
            DMADescriptor(unsigned char id, void *obj, std::size_t obj_size) : id(id), obj(obj), obj_size(obj_size)
            {
                if (obj_size % 3 != 0)
                    throw SFA::util::logic_error("Invalid DMAObject size", __FILE__, __func__);
                const auto idleId = static_cast<unsigned long>(((idleState() << 2) >> 2).to_ulong());
                if (id == idleId)
                    throw SFA::util::logic_error("DMADescriptor id is reserved for the serial line idle state", __FILE__, __func__);
                const auto shutdownId = static_cast<unsigned long>(((shutdownState() << 2) >> 2).to_ulong());
                if (id == shutdownId)
                    throw SFA::util::logic_error("DMADescriptor id is reserved for the com_shutdown request on idle", __FILE__, __func__);
                const auto poweronId = static_cast<unsigned long>(((poweronState() << 2) >> 2).to_ulong());
                if (id == poweronId)
                    throw SFA::util::logic_error("DMADescriptor id is reserved for the poweron notification", __FILE__, __func__);
            }
            unsigned char id = 0xFF;
            void *obj = nullptr;
            std::size_t obj_size = 0;
            //bool readLock = false; // serial priority checks for readLock; subcontroller<subcontroller> read checks for readLock
            bool synced = true;    // subcontroller transfer checks for synced
            int rx_counter = 0;    // DEBUG
            int tx_counter = 0;    // DEBUG
        };
        template <unsigned int N>
        struct DescriptorHelper : public std::array<DMADescriptor, N>
        {
        public:
            using std::array<DMADescriptor, N>::array;
            template <typename... T>
            void operator()(T &...obj_ref)
            {
                (assign(obj_ref), ...);
            }

        private:
            template <typename T>
            void assign(T &obj_ref)
            {
                (*this)[count] = DMADescriptor(static_cast<unsigned char>(count), reinterpret_cast<void *>(&obj_ref), sizeof(obj_ref));
                count++;
            }
            std::size_t count = 0;
        };
    }
    namespace MemoryView
    {
        struct DestinationAndOrigin : private SOS::MemoryView::TaskCable<std::size_t, 2>
        {
            DestinationAndOrigin() : SOS::MemoryView::TaskCable<std::size_t, 2>{0, 0} {}
            auto &getReadDestinationRef() { return std::get<0>(*this); }
            auto &getWriteOriginRef() { return std::get<1>(*this); }
        };
        template <typename... Objects>
        struct SerialProcessNotifier : public SOS::MemoryView::BusShaker
        {
            using cables_type = std::tuple<DestinationAndOrigin>;
            SerialProcessNotifier()
            {
                std::apply(descriptors, objects); // ALWAYS: Initialize Descriptors in Constructor
            }
            cables_type cables;
            std::tuple<Objects...> objects{};
            SOS::Protocol::DescriptorHelper<std::tuple_size<std::tuple<Objects...>>::value> descriptors{};
            auto &readDestination() { return std::get<0>(cables).getReadDestinationRef(); }
            auto &writeOrigin() { return std::get<0>(cables).getWriteOriginRef(); }
        };
    }
    namespace Behavior
    {
        class SerialProcessing
        {
        public:
            SerialProcessing() {}
            void event_loop()
            {
                while (is_running())
                {
                    if (transfered())
                    {
                        write_notify_hook();
                    }
                    if (received())
                    {
                        read_notify_hook();
                    }
                    std::this_thread::yield();
                }
                finished();
            }

        protected:
            virtual void start() = 0;
            virtual bool is_running() = 0;
            virtual void finished() = 0;
            virtual bool received() = 0;
            virtual bool transfered() = 0;
            virtual void write_notify_hook() = 0;
            virtual void read_notify_hook() = 0;
        };
    }
    namespace Protocol
    {
        template <typename... Objects>
        class Serial
        { // write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
        public:
            Serial() {}
            virtual void event_loop()
            { // final
                int read4minus1 = 0;
                int write3plus1 = 0;
                while (is_running())
                {
                    if (handshake())
                    {
                        if (com_shutdown && sent_com_shutdown)
                            finished_com_shutdown = true;
                        if (!receive_lock)
                            read_hook();
                        else
                            read_object(read4minus1);
                        if (!send_lock)
                            write_hook();
                        if (send_lock)
                            write_object(write3plus1);
                        handshake_ack();
                    }
                    if (loop_shutdown && finished_com_shutdown)
                        com_shutdown_action();
                    std::this_thread::yield();
                }
                finished();
                // std::cout<<typeid(*this).name()<<" shutdown"<<std::endl;
            }

        protected:
            virtual bool is_running() = 0;
            virtual void finished() = 0;
            virtual bool handshake() = 0;
            virtual void handshake_ack() = 0;
            virtual void send_acknowledge() = 0;    // 3
            virtual void send_request() = 0;        // 1
            virtual bool receive_request() = 0;     // 2
            virtual bool receive_acknowledge() = 0; // 4
            virtual unsigned char read_byte() = 0;
            virtual void write_byte(unsigned char) = 0;
            virtual void com_hotplug_action() = 0;//send_lock: check for objects not finished sending
            //read_lock: Use an encapsulated messaging method to let the other side handle its incorrect shutdown / power loss
            virtual void com_shutdown_action() = 0;//no lock checks; request_stop or hotplugging?
            void resend_current_object()
            {
                if (send_lock || writeCount != 0){
                    throw SFA::util::runtime_error("Poweron after unexpected shutdown.", __FILE__, __func__);
                    send_lock = true;
                    writeCount = 0;
                    writeOriginPos = 0;
                }
            }
            void clear_read_receive()
            {
                if (receive_lock || readCount != 0){
                    throw SFA::util::runtime_error("Hotplug after unexpected shutdown.", __FILE__, __func__);
                    for (std::size_t j = 0; j < foreign().descriptors.size(); j++)
                    {
                        //if (foreign().descriptors[j].readLock)
                        if (foreign().readDestination().load() == j)
                        {
                            throw SFA::util::runtime_error("Object could be outdated. Corrupted unless resend_current_object is called from the other side.", __FILE__, __func__);
                        }
                    }
                    receive_lock = false;
                    readCount = 0;
                    readDestinationPos = 0;
                }
            };
            bool mcu_updated = false;      // mcu_write,fpga_read bit 7
            bool fpga_acknowledge = false; // mcu_write,fpga_read bit 6
            bool fpga_updated = false;     // mcu_read,fpga_write bit 7
            bool mcu_acknowledge = false;  // mcu_read,fpga_write bit 6
            virtual constexpr typename SOS::MemoryView::SerialProcessNotifier<Objects...> &foreign() = 0;
            bool loop_shutdown = false;
        private:
            bool first_run = true;
            bool com_shutdown = false;
            bool sent_com_shutdown = false;
            bool finished_com_shutdown = false;
            bool receive_lock = false;
            bool send_lock = false;
            void read_hook()
            {
                unsigned char data = read_byte();
                read_bits(static_cast<unsigned long>(data));
                if (receive_request())
                {
                    std::bitset<8> obj_id = static_cast<unsigned long>(data);
                    obj_id = (obj_id << 2) >> 2;
                    if (obj_id == ((poweronState() << 2) >> 2))
                    {
                        if (com_shutdown){
                            if (!finished_com_shutdown){
                                throw SFA::util::logic_error("Power on with pending com_shutdown.", __FILE__, __func__);
                            } else {
                                //throw SFA::util::logic_error("Power on with completed com_shutdown.", __FILE__, __func__);
                            }
                        }
                        com_shutdown = false;
                        sent_com_shutdown = false;
                        finished_com_shutdown = false;
                        com_hotplug_action();//NO send_request() or send_acknowledge() in here
                        //start_calc_thread
                    }
                    else if (obj_id == ((idleState() << 2) >> 2))
                    {
                        if (finished_com_shutdown){
                            throw SFA::util::logic_error("Spurious handshake.", __FILE__, __func__);
                        }
                    }
                    else if (obj_id == ((shutdownState() << 2) >> 2))
                    {
                        if (!first_run)
                            com_shutdown = true; // incoming
                        //stop_calc_thread
                        // std::cout<<typeid(*this).name();
                        // std::cout<<"O";
                    }
                    else
                    {
                        if (com_shutdown) {
                            //throw SFA::util::logic_error("Transfer requested after com_shutdown", __FILE__, __func__);
                        } else {
                            auto id = static_cast<unsigned char>(obj_id.to_ulong());
                            for (std::size_t j = 0; j < foreign().descriptors.size(); j++)
                            {
                                if (foreign().descriptors[j].synced == true && foreign().descriptors[j].id == id)
                                {
                                    receive_lock = true;
                                    //foreign().descriptors[j].readLock = true;
                                    foreign().readDestination().store(id);
                                    // std::cout<<typeid(*this).name()<<" starting ReadDestination "<<foreign().readDestination()<<std::endl;
                                    readDestinationPos = 0;
                                    send_acknowledge(); // DANGER: change writted state has to be after read_bits
                                }
                            }
                        }
                    }
                    // else {
                    // std::cout<<typeid(*this).name();
                    // std::cout<<".";
                    //}
                }
            }
            void write_hook()
            {
                if (receive_acknowledge())
                {
                    if (first_run)
                        throw SFA::util::logic_error("Received transfer acknowledge during poweron.", __FILE__, __func__);
                    send_lock = true;
                    foreign().signal.getAcknowledgeRef().clear(); // Used as separate signals, not a handshake
                }
                else
                {
                    // read in handshake -> set wire to valid state
                    if (first_run){
                        auto id = poweronState();
                        write_bits(id);
                        id.set(7, 1); // override write_bits
                        std::cout<<typeid(*this).name();
                        std::cout<<"P";
                        write_byte(static_cast<unsigned char>(id.to_ulong()));
                        first_run=false;
                    } else {
                    if (!getFirstSyncObject())
                    {
                        if (!loop_shutdown && !com_shutdown){//write an idle
                            auto id = idleState();
                            write_bits(id);
                            id.set(7, 1); // override write_bits
                            // std::cout<<typeid(*this).name();
                            // std::cout<<"!";
                            write_byte(static_cast<unsigned char>(id.to_ulong()));
                        } else {
                            auto id = shutdownState();
                            write_bits(id);
                            id.set(7, 1); // override write_bits
                            std::cout << typeid(*this).name();
                            std::cout << "X";
                            write_byte(static_cast<unsigned char>(id.to_ulong()));
                            sent_com_shutdown = true;
                        }
                    }
                    }
                }
            }
            unsigned int writeCount = 0; // write3plus1
            std::size_t writeOriginPos = 0;
            unsigned int readCount = 0; // read4minus1
            std::size_t readDestinationPos = 0;
            bool getFirstSyncObject()
            {
                bool gotOne = false;
                for (std::size_t i = 0; i < foreign().descriptors.size() && !gotOne; i++)
                {
                    //if (foreign().descriptors[i].readLock && !foreign().descriptors[i].synced)
                    if (receive_lock && i==foreign().readDestination().load() && !foreign().descriptors[i].synced)
                        throw SFA::util::logic_error("DMAObject has entered an illegal sync state.", __FILE__, __func__);
                    if (!foreign().descriptors[i].synced)
                    {
                        send_request();
                        foreign().writeOrigin().store(foreign().descriptors[i].id);
                        writeOriginPos = 0;
                        std::bitset<8> id;
                        write_bits(id);
                        std::bitset<8> obj_id = static_cast<unsigned long>(foreign().writeOrigin().load()); // DANGER: overflow check
                        id = id ^ obj_id;
                        // std::cout<<typeid(*this).name()<<" sending WriteOrigin "<<foreign().writeOrigin()<<std::endl;
                        write_byte(static_cast<unsigned char>(id.to_ulong()));
                        gotOne = true;
                    }
                }
                return gotOne;
            }
            void write_object(int &write3plus1)
            {
                if (write3plus1 < 3)
                {
                    unsigned char data;
                    data = reinterpret_cast<char *>(foreign().descriptors[foreign().writeOrigin().load()].obj)[writeOriginPos++];
                    write(data);
                    write3plus1++;
                }
                else
                { // write3plus1==3
                    if (writeOriginPos == foreign().descriptors[foreign().writeOrigin().load()].obj_size)
                    {
                        foreign().descriptors[foreign().writeOrigin().load()].synced = true;
                        send_lock = false;
                        writeOriginPos = 0;
                        foreign().descriptors[foreign().writeOrigin().load()].tx_counter++; // DEBUG
                        // std::cout<<typeid(*this).name();
                        // std::cout<<"$";
                    }
                    write(63); //'?' empty write
                    write3plus1 = 0;
                }
            }
            void read_object(int &read4minus1)
            {
                unsigned char data = read_byte();
                read_bits(static_cast<unsigned long>(data));
                read(data);
                if (read4minus1 < 3)
                {
                    read4minus1++;
                }
                else if (read4minus1 == 3)
                {
                    auto read3bytes = read_flush();
                    if (readDestinationPos == foreign().descriptors[foreign().readDestination().load()].obj_size)
                    {
                        //foreign().descriptors[foreign().readDestination().load()].readLock = false;
                        receive_lock = false;
                        foreign().signal.getUpdatedRef().clear();
                        foreign().descriptors[foreign().readDestination().load()].rx_counter++; // DEBUG
                    }
                    else
                    {
                        for (std::size_t i = 0; i < 3; i++)
                        {
                            reinterpret_cast<char *>(foreign().descriptors[foreign().readDestination().load()].obj)[readDestinationPos++] = read3bytes[i];
                        }
                    }
                    read4minus1 = 0;
                }
            }
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
                    ;
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
            virtual void write_bits(std::bitset<8> &out) = 0;
            static std::bitset<8> write_recover(decltype(writeAssembly) &writeAssembly, decltype(writeCount) &writeCount, std::bitset<8> &out)
            {
                std::bitset<8> cache;
                cache = writeAssembly[writeCount - 1] << (4 - writeCount) * 2;
                cache = cache >> 1 * 2;
                return out ^ cache;
            }
            virtual void read_bits(std::bitset<8> temp) = 0;
            static void read_shift(decltype(readAssembly) &readAssembly, decltype(readCount) &readCount, std::bitset<24> &temp)
            {
                temp = temp << (4 - 0) * 4 + 2;     // split off 1st 2bit
                temp = temp >> (readCount * 3) * 2; // shift
                readAssembly = readAssembly ^ temp; // overlay
            }
        };
        template <typename... Objects>
        class SerialFPGA : protected virtual Serial<Objects...>
        {
        public:
            SerialFPGA() {}

        private:
            virtual void read_bits(std::bitset<8> temp) final
            {
                Serial<Objects...>::mcu_updated = temp[7];
                Serial<Objects...>::fpga_acknowledge = temp[6];
                Serial<Objects...>::mcu_acknowledge = false;
            }
            virtual void write_bits(std::bitset<8> &out) final
            {
                if (Serial<Objects...>::fpga_updated)
                    out.set(7, 1);
                else
                    out.set(7, 0);
                if (Serial<Objects...>::mcu_acknowledge)
                    out.set(6, 1);
                else
                    out.set(6, 0);
            }
            virtual void send_acknowledge() final
            {
                if (Serial<Objects...>::mcu_updated)
                {
                    Serial<Objects...>::mcu_acknowledge = true;
                }
            }
            virtual void send_request() final
            {
                Serial<Objects...>::fpga_updated = true;
            }
            virtual bool receive_acknowledge() final
            {
                if (Serial<Objects...>::fpga_acknowledge)
                {
                    Serial<Objects...>::fpga_updated = false;
                    return true;
                }
                return false;
            }
            virtual bool receive_request() final
            {
                return Serial<Objects...>::mcu_updated;
            }
        };
        template <typename... Objects>
        class SerialMCU : protected virtual Serial<Objects...>
        {
        public:
            SerialMCU() {}

        private:
            virtual void read_bits(std::bitset<8> temp) final
            {
                Serial<Objects...>::fpga_updated = temp[7];
                Serial<Objects...>::mcu_acknowledge = temp[6];
                Serial<Objects...>::fpga_acknowledge = false;
            }
            virtual void write_bits(std::bitset<8> &out) final
            {
                if (Serial<Objects...>::mcu_updated)
                    out.set(7, 1);
                else
                    out.set(7, 0);
                if (Serial<Objects...>::fpga_acknowledge)
                    out.set(6, 1);
                else
                    out.set(6, 0);
            }
            virtual void send_acknowledge() final
            {
                if (Serial<Objects...>::fpga_updated)
                {
                    Serial<Objects...>::fpga_acknowledge = true;
                }
            }
            virtual void send_request() final
            {
                Serial<Objects...>::mcu_updated = true;
            }
            virtual bool receive_acknowledge() final
            {
                if (Serial<Objects...>::mcu_acknowledge)
                {
                    Serial<Objects...>::mcu_updated = false;
                    return true;
                }
                return false;
            }
            virtual bool receive_request() final
            {
                return Serial<Objects...>::fpga_updated;
            }
        };
    }
}
