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
            bool readLock = false; // serial priority checks for readLock; subcontroller<subcontroller> read checks for readLock
            bool transfer = false; // serial priority checks for readLockAck
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
            auto &getReceiveNotificationRef() { return std::get<0>(*this); }
            auto &getSendNotificationRef() { return std::get<1>(*this); }
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
            auto &receiveNotificationId() { return std::get<0>(cables).getReceiveNotificationRef(); }
            auto &sendNotificationId() { return std::get<0>(cables).getSendNotificationRef(); }
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
                        if (first_run){
                            //read_hook, (incoming request and acknowledge) are ignored.
                            //if both are sending a poweron at the same time, only the second is read by the other party.
                            send_poweronRequest();
                        } else {
                            unsigned char data = read_byte();
                            read_bits(static_cast<unsigned long>(data));
                            if (receive_request())
                                read_hook(data);
                            else
                                read_object(read4minus1,data);
                            acknowledge_hook();
                            if (write_hook())
                                write_object(write3plus1);
                        }
                        handshake_ack();
                    }
                    if (loop_shutdown && transmission_received && save_completed)
                        shutdown_action();
                    std::this_thread::yield();
                }
                finished();
                /*for (std::size_t j = 0; j < foreign().descriptors.size(); j++){
                    if (foreign().descriptors[j].readLock)
                        throw SFA::util::runtime_error("ReadLocked item after thread exit", __FILE__, __func__);
                }*/
                std::cout<<typeid(*this).name()<<" shutdown"<<std::endl;
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
            virtual void com_shutdown_action() = 0;
            virtual void dangling_idle_action() = 0;
            virtual void save_completed_action() = 0;//stop processing data
            virtual void shutdown_action() = 0;//no lock checks; request_stop or hotplugging
            void resend_current_object()
            {
                if (send_lock || writeCount != 0){
                    throw SFA::util::runtime_error("Poweron after unexpected shutdown.", __FILE__, __func__);
                    foreign().descriptors[writeOrigin].synced = false;
                    foreign().descriptors[writeOrigin].transfer = false;
                }
            }
            void clear_read_receive()
            {
                if (receive_lock || readCount != 0){
                    throw SFA::util::runtime_error("Hotplug after unexpected shutdown.", __FILE__, __func__);
                    for (std::size_t j = 0; j < foreign().descriptors.size(); j++)
                    {
                        if (foreign().descriptors[j].readLock)
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
            bool received_com_shutdown = false;
            bool sent_com_shutdown = false;
            bool save_completed = false;
            bool transmission_received = false;
            std::size_t acknowledgeId = 255;
            bool acknowledgeRequested = false;
            bool receive_lock = false;
            bool send_lock = false;
            void read_hook(unsigned char &data)
            {
                    std::bitset<8> obj_id = static_cast<unsigned long>(data);
                    obj_id = (obj_id << 2) >> 2;
                    if (obj_id == ((poweronState() << 2) >> 2))
                    {
                        if (received_com_shutdown){
                            if (!save_completed){
                                throw SFA::util::logic_error("Power on with pending com_shutdown.", __FILE__, __func__);
                            } else {
                                //throw SFA::util::logic_error("Power on with completed com_shutdown.", __FILE__, __func__);
                            }
                        }
                        received_com_shutdown = false;
                        sent_com_shutdown = false;
                        save_completed = false;
                        com_hotplug_action();//NO send_request() or send_acknowledge() in here
                        //start_calc_thread
                    }
                    else if (obj_id == ((idleState() << 2) >> 2))
                    {
                        if (received_com_shutdown && !transmission_received){
                            /*for (std::size_t j = 0; j < foreign().descriptors.size(); j++){
                                if (foreign().descriptors[j].readLock)
                                    throw SFA::util::logic_error("There should not be any idle coming in when there are unsynced objects.", __FILE__, __func__);
                            }*/
                            dangling_idle_action();
                            transmission_received = true;
                        }
                    }
                    else if (obj_id == ((shutdownState() << 2) >> 2))
                    {
                        //stop_calc_thread
                        com_shutdown_action();
                        received_com_shutdown = true; // incoming
                        // std::cout<<typeid(*this).name();
                        // std::cout<<"O";
                    }
                    else
                    {
                        auto id = static_cast<unsigned char>(obj_id.to_ulong());
                        for (std::size_t j = 0; j < foreign().descriptors.size(); j++)
                        {
                            if (foreign().descriptors[j].id == id){
                                if (foreign().descriptors[j].readLock)
                                    throw SFA::util::runtime_error("Duplicate readLock request",__FILE__,__func__);
                                if (foreign().descriptors[j].synced)
                                {
                                    if (!foreign().descriptors[j].transfer){
                                    foreign().descriptors[j].readLock = true;
                                    send_acknowledge();//ALWAYS: use write_bits to set request and acknowledge flags
                                    } else {
                                        throw SFA::util::logic_error("Incoming readLock is rejected!",__FILE__,__func__);//The other side has to cope with it
                                    }
                                } else
                                {
                                    if (!foreign().descriptors[j].transfer){
                                        if (acknowledgeRequested)
                                            throw SFA::util::runtime_error("Incoming readLock is canceling local transfer request",__FILE__,__func__);
                                        throw SFA::util::runtime_error("Incoming readLock is canceling local write operation",__FILE__,__func__);
                                        foreign().descriptors[j].readLock = true;
                                        foreign().descriptors[j].synced = true;//OVERRIDE
                                        acknowledgeId = 255;
                                        acknowledgeRequested = false;
                                        send_acknowledge();//ALWAYS: use write_bits to set request and acknowledge flags
                                    } else {
                                        throw SFA::util::logic_error("Incoming readLock is rejected!",__FILE__,__func__);//The other side has to cope with it
                                    }
                                }
                            }
                        }
                    }
            }
            void send_poweronRequest() {
                auto id = poweronState();
                send_request();
                write_bits(id);
                std::cout<<typeid(*this).name();
                std::cout<<"P";
                write_byte(static_cast<unsigned char>(id.to_ulong()));
                first_run=false;
            }
            void send_comshutdownRequest() {
                auto id = shutdownState();
                send_request();
                write_bits(id);
                std::cout << typeid(*this).name();
                std::cout << "X";
                write_byte(static_cast<unsigned char>(id.to_ulong()));
                sent_com_shutdown = true;
            }
            void send_idleRequest() {
                auto id = idleState();
                send_request();
                write_bits(id);
                // std::cout<<typeid(*this).name();
                // std::cout<<"!";
                write_byte(static_cast<unsigned char>(id.to_ulong()));
            }
            void send_transferRequest(decltype(DMADescriptor::id) unsynced){
                std::bitset<8> id;
                send_request();
                write_bits(id);
                std::bitset<8> obj_id = static_cast<unsigned long>(unsynced); // DANGER: overflow check
                id = id ^ obj_id;
                //std::cout<<typeid(*this).name();
                //std::cout<<":"<<(unsigned int)unsynced<<std::endl;//why not ID?!
                write_byte(static_cast<unsigned char>(id.to_ulong()));
            }
            void acknowledge_hook() {
                if (receive_acknowledge())
                {
                    //std::cout<<typeid(*this).name();
                    //std::cout<<"."<<acknowledgeId<<std::endl;
                    if (!acknowledgeRequested){
                        throw SFA::util::logic_error("Acknowledge received without any request.", __FILE__, __func__);
                    } else {
                        bool gotOne = false;
                        for (std::size_t j = 0; j < foreign().descriptors.size(); j++){
                            if (j == acknowledgeId){
                                if (foreign().descriptors[j].synced)
                                    throw SFA::util::logic_error("Received a transfer request on synced object",__FILE__,__func__);
                                if (foreign().descriptors[j].readLock)
                                    throw SFA::util::logic_error("Received a transfer request on readLocked object",__FILE__,__func__);
                                foreign().descriptors[j].transfer = true;//SUCCESS
                                acknowledgeId = 255;
                                acknowledgeRequested = false;
                                gotOne = true;
                            }
                        }
                        if (!gotOne)
                            throw SFA::util::logic_error("AcknowledgeId does not reference a valid object.", __FILE__, __func__);
                    }
                } else {
                    if (acknowledgeRequested){
                        throw SFA::util::runtime_error("Previous transfer has not been acknowledged.", __FILE__, __func__);
                        bool gotOne = false;
                        for (std::size_t j = 0; j < foreign().descriptors.size(); j++){
                            if (j == acknowledgeId){
                                if (!foreign().descriptors[j].readLock)
                                    throw SFA::util::logic_error("The only reason for not getting an acknowledge is that a readLock has already been issued by the other side",__FILE__,__func__);
                                if (foreign().descriptors[j].synced)
                                    throw SFA::util::logic_error("Invalid acknowledgeId",__FILE__,__func__);
                                throw SFA::util::runtime_error("The other side has overridden sync priority",__FILE__,__func__);
                                foreign().descriptors[j].synced = true;
                                foreign().descriptors[j].transfer = false;//FAIL
                                acknowledgeId = 255;
                                acknowledgeRequested = false;
                                gotOne = true;
                            }
                        }
                        if (!gotOne)
                            throw SFA::util::logic_error("AcknowledgeId does not reference a valid object.", __FILE__, __func__);
                    }
                }
            }
            unsigned int writeCount = 0; // write3plus1
            std::size_t writeOrigin = 255;
            std::size_t writeOriginPos = 0;
            unsigned int readCount = 0; // read4minus1
            std::size_t readDestination = 255;
            std::size_t readDestinationPos = 0;
            bool getFirstTransfer()
            {
                bool gotOne = false;
                for (std::size_t j = 0; j < foreign().descriptors.size() && !gotOne; j++){
                    if (!foreign().descriptors[j].synced && !foreign().descriptors[j].transfer){
                        if (foreign().descriptors[j].readLock)
                            throw SFA::util::logic_error("Synced status has not been overriden when readLock was acquired.", __FILE__, __func__);
                        acknowledgeId = j;//gets overridden at baud rate
                        acknowledgeRequested = true;
                        send_transferRequest(foreign().descriptors[j].id);
                        gotOne = true;
                    }
                }
                return gotOne;
            }
            bool getFirstSyncObject()
            {
                bool gotOne = false;
                    for (std::size_t j = 0; j < foreign().descriptors.size() && !gotOne; j++)
                    {
                        if (foreign().descriptors[j].readLock && !foreign().descriptors[j].synced)
                            throw SFA::util::logic_error("DMAObject has entered an illegal sync state.", __FILE__, __func__);
                        if (foreign().descriptors[j].transfer)
                        {
                            if (foreign().descriptors[j].synced)
                                throw SFA::util::logic_error("Found a transfer object which is synced",__FILE__,__func__);
                            if (foreign().descriptors[j].readLock)
                                throw SFA::util::logic_error("Found a transfer object which is readLocked",__FILE__,__func__);
                            if (writeOriginPos != 0)
                                throw SFA::util::logic_error("WRITEERROR",__FILE__,__func__);
                                //std::cout<<typeid(*this).name()<<"WRITEERROR"<<writeOrigin<<std::endl;
                            if ((writeOrigin == readDestination) && writeOrigin != 255 && readDestination != 255)
                                throw SFA::util::logic_error("Object can not be read and written at the same time!",__FILE__,__func__);
                            send_lock = true;
                            writeOrigin = j;
                            std::cout<<typeid(*this).name()<<"WO"<<writeOrigin<<std::endl;
                            gotOne = true;
                        }
                    }
                return gotOne;
            }
            bool write_hook(){
                bool send_complete = false;
                if (getFirstTransfer()) {//NOT SAVE, double write
                    send_complete = true;
                } else {
                    if ((received_com_shutdown && !sent_com_shutdown) || (loop_shutdown && !sent_com_shutdown)){
                        send_comshutdownRequest();//NOT SAFE, double write
                        send_complete = true;
                    } else {
                        if (!send_lock){
                            if (!getFirstSyncObject()) {
                                if (sent_com_shutdown) {
                                    save_completed_action();
                                    for (std::size_t j = 0; j < foreign().descriptors.size(); j++){
                                        if (foreign().descriptors[j].synced == false)
                                            throw SFA::util::logic_error("Unsynced object after child stopped",__FILE__,__func__);
                                    }
                                    save_completed = true;
                                }
                                // read in handshake -> set wire to valid state
                                send_idleRequest();//SAFE, still no send_lock
                                send_complete = true;
                            }
                        }
                    }
                }
                return send_complete;
            }
            void write_object(int &write3plus1)
            {
                if (send_lock){
                if (write3plus1 < 3)
                {
                    unsigned char data;
                    data = reinterpret_cast<char *>(foreign().descriptors[writeOrigin].obj)[writeOriginPos++];
                    write(data);
                    write3plus1++;
                }
                else
                { // write3plus1==3
                    if (writeOriginPos == foreign().descriptors[writeOrigin].obj_size)
                    {
                        foreign().descriptors[writeOrigin].transfer = false;
                        foreign().descriptors[writeOrigin].synced = true;
                        send_lock = false;
                        foreign().sendNotificationId().store(writeOrigin);
                        foreign().signal.getAcknowledgeRef().clear();//Used as separate signals, not a handshake
                        foreign().descriptors[writeOrigin].tx_counter++; // DEBUG
                        // std::cout<<typeid(*this).name();
                        // std::cout<<"$";
                        writeOrigin = 255;
                        writeOriginPos = 0;
                    }
                    write(63); //'?' empty write
                    write3plus1 = 0;
                }
                }
            }
            void read_object(int &read4minus1, unsigned char &data)
            {
                if (!receive_lock){
                    bool gotOne = false;
                    for (std::size_t j = 0; j < foreign().descriptors.size() && !gotOne; j++){
                        if (foreign().descriptors[j].readLock){
                            if (readDestinationPos != 0){
                                throw SFA::util::logic_error("READERROR",__FILE__,__func__);
                                //std::cout<<typeid(*this).name()<<"READERROR"<<readDestination<<std::endl;
                            }
                            if ((writeOrigin == readDestination) && writeOrigin != 255 && readDestination != 255)
                                throw SFA::util::logic_error("Object can not be read and written at the same time!",__FILE__,__func__);
                            receive_lock = true;
                            readDestination = j;
                            std::cout<<typeid(*this).name()<<"RD"<<readDestination<<std::endl;
                            gotOne = true;
                        }
                    }
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
                    if (readDestinationPos == foreign().descriptors[readDestination].obj_size)
                    {
                        foreign().descriptors[readDestination].readLock = false;
                        receive_lock = false;
                        foreign().receiveNotificationId().store(readDestination);
                        foreign().signal.getUpdatedRef().clear();//Used as separate signals, not a handshake
                        foreign().descriptors[readDestination].rx_counter++; // DEBUG
                        readDestination = 255;
                        readDestinationPos = 0;
                    }
                    else
                    {
                        for (std::size_t i = 0; i < 3; i++)
                        {
                            reinterpret_cast<char *>(foreign().descriptors[readDestination].obj)[readDestinationPos++] = read3bytes[i];
                        }
                    }
                    read4minus1 = 0;
                }
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
