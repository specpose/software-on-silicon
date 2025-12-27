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
            id.set(0, 1);
            return id; //-> "10111101"
        }
        static std::bitset<8> sighupState()
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
        struct com_vars {
            bool loop_shutdown = false;
            bool received_idle = false;
            bool received_com_shutdown = false;
            bool sent_com_shutdown = false;
            bool received_sighup = false;
            bool sent_sighup = false;
            bool acknowledgeRequested = false;
            bool received_acknowledge = false;
        };
        struct DMADescriptor
        {
            DMADescriptor() {} // DANGER
            DMADescriptor(unsigned char id, void *obj, std::size_t obj_size) : id(id), obj(obj), obj_size(obj_size)
            {
                if (obj_size % 3 != 0)
                    SFA::util::logic_error(SFA::util::error_code::InvalidDMAObjectSize, __FILE__, __func__, typeid(*this).name());
                const auto idleId = static_cast<unsigned long>(((idleState() << 2) >> 2).to_ulong());
                if (id == idleId)
                    SFA::util::logic_error(SFA::util::error_code::DMADescriptorIdIsReservedForTheSerialLineIdleState, __FILE__, __func__, typeid(*this).name());
                const auto shutdownId = static_cast<unsigned long>(((shutdownState() << 2) >> 2).to_ulong());
                if (id == shutdownId)
                    SFA::util::logic_error(SFA::util::error_code::DMADescriptorIdIsReservedForTheComShutdownRequestOnIdle, __FILE__, __func__, typeid(*this).name());
                const auto poweronId = static_cast<unsigned long>(((poweronState() << 2) >> 2).to_ulong());
                if (id == poweronId)
                    SFA::util::logic_error(SFA::util::error_code::DMADescriptorIdIsReservedForThePoweronNotification, __FILE__, __func__, typeid(*this).name());
                const auto sighupId = static_cast<unsigned long>(((sighupState() << 2) >> 2).to_ulong());
                if (id == sighupId)
                    SFA::util::logic_error(SFA::util::error_code::DMADescriptorIdIsReservedForTheReadsFinishedNotification, __FILE__, __func__, typeid(*this).name());
            }
            unsigned char id = 0xFF;
            void *obj = nullptr;
            std::size_t obj_size = 0;
            volatile bool readLock = false; // SerialProcessing thread
            bool queued = false;
            volatile bool synced = true;    // SerialProcessing thread
            bool transfer = false;
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
                    std::this_thread::yield();
                    if (handshake())
                    {
                        //IN
                            if (!first_run) {
                            unsigned char data = read_byte();
                            read_bits(static_cast<unsigned long>(data));
                            _vars.received_acknowledge = receive_acknowledge();
                            received_request = receive_request();
                            _vars.received_idle = false;
                            if (received_request)
                                read_hook(data);
                            else
                                read_object(read4minus1,data);
                            transfer_hook();
                            acknowledge_hook();
                            }
                        //OUT
                            if (!write_hook())
                                if (!write_object(write3plus1))
                                    send_idleRequest();
                        handshake_ack();
                    }
                    if (exit_query() && _vars.loop_shutdown)
                        shutdown_action();
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
            virtual void stop_notifier() = 0;
            virtual void com_shutdown_action() = 0;
            virtual void com_sighup_action() = 0;
            virtual bool exit_query() = 0;
            virtual bool incoming_shutdown_query() = 0;
            virtual bool outgoing_sighup_query() = 0;
            virtual void shutdown_action() = 0;//no lock checks; request_stop or hotplugging
            void resend_current_object()
            {
                if (send_lock || writeCount != 0){
                    SFA::util::runtime_error(SFA::util::error_code::PoweronAfterUnexpectedShutdown, __FILE__, __func__, typeid(*this).name());
                    foreign().descriptors[writeOrigin].synced = false;
                    foreign().descriptors[writeOrigin].transfer = false;
                    send_lock = false;
                    writeCount = 0;
                    writeOriginPos = 0;
                }
            }
            void clear_read_receive()
            {
                if (receive_lock || readCount != 0){
                    SFA::util::runtime_error(SFA::util::error_code::HotplugAfterUnexpectedShutdown, __FILE__, __func__, typeid(*this).name());
                    for (std::size_t j = 0; j < foreign().descriptors.size(); j++)
                    {
                        if (foreign().descriptors[j].readLock)
                        {
                            SFA::util::runtime_error(SFA::util::error_code::ObjectCouldBeOutdated, __FILE__, __func__, typeid(*this).name());
                        }
                    }
                    receive_lock = false;
                    readCount = 0;
                    readDestinationPos = 0;
                }
            };
            bool transfers_pending(){
                for (std::size_t j = 0; j < foreign().descriptors.size(); j++){
                    if (!foreign().descriptors[j].synced && !foreign().descriptors[j].transfer)
                        return true;
                }
                return false;
            }
            bool writes_pending(){
                for (std::size_t j = 0; j < foreign().descriptors.size(); j++){
                    if (foreign().descriptors[j].transfer)
                        return true;
                }
                return false;
            }
            bool reads_pending(){
                for (std::size_t j = 0; j < foreign().descriptors.size(); j++){
                    if (foreign().descriptors[j].readLock)
                        return true;
                }
                return false;
            }
            bool mcu_updated = false;      // mcu_write,fpga_read bit 7
            bool fpga_acknowledge = false; // mcu_write,fpga_read bit 6
            bool fpga_updated = false;     // mcu_read,fpga_write bit 7
            bool mcu_acknowledge = false;  // mcu_read,fpga_write bit 6
            virtual constexpr typename SOS::MemoryView::SerialProcessNotifier<Objects...> &foreign() = 0;
            com_vars _vars = com_vars{};
        private:
            bool first_run = true;
            bool received_request = false;
            unsigned char requestId = 255;
            unsigned char acknowledgeId = 255;
            bool receive_lock = false;
            bool send_lock = false;
            void read_hook(unsigned char &data)
            {
                    std::bitset<8> obj_id = static_cast<unsigned long>(data);
                    obj_id = (obj_id << 2) >> 2;
                    if (obj_id == ((poweronState() << 2) >> 2))
                    {
                        //if (!_vars.received_sighup)
                        //    SFA::util::runtime_error(SFA::util::error_code::PreviousCommunicationNotSighupTerminated, __FILE__, __func__, typeid(*this).name());
                        if (_vars.acknowledgeRequested || _vars.received_acknowledge)
                            SFA::util::runtime_error(SFA::util::error_code::PreviousTransferRequestsWereNotCleared, __FILE__, __func__, typeid(*this).name());
                        _vars = com_vars{};
                        com_hotplug_action();//NO send_request() or send_acknowledge() in here
                        //start_calc_thread
                        //start notifier
                    }
                    else if (obj_id == ((idleState() << 2) >> 2))
                    {
                        _vars.received_idle = true;
                        if (_vars.received_com_shutdown)
                            std::cout<<typeid(*this).name()<<"."<<"!"<<std::endl;
                    }
                    else if (obj_id == ((shutdownState() << 2) >> 2))
                    {
                        if (_vars.received_sighup)
                            SFA::util::logic_error(SFA::util::error_code::NotIdleAfterSighup, __FILE__, __func__, typeid(*this).name());
                        if (!_vars.received_com_shutdown){
                            com_shutdown_action();
                            _vars.received_com_shutdown = true;
                            std::cout<<typeid(*this).name()<<"."<<"X"<<std::endl;
                        } else {
                            SFA::util::logic_error(SFA::util::error_code::DuplicateComShutdown,__FILE__,__func__, typeid(*this).name());
                        }
                    }
                    else if (obj_id == ((sighupState() << 2) >> 2))
                    {
                        if (!_vars.received_sighup)
                        {
                            com_sighup_action();
                            _vars.received_sighup = true;
                        } else {
                            SFA::util::logic_error(SFA::util::error_code::DuplicateSighup,__FILE__,__func__, typeid(*this).name());
                        }
                    }
                    else
                    {
                        if (_vars.received_sighup)
                            SFA::util::logic_error(SFA::util::error_code::NotIdleAfterSighup, __FILE__, __func__, typeid(*this).name());
                        requestId = static_cast<unsigned char>(obj_id.to_ulong());
                    }
            }
            void send_poweronRequest() {
                auto id = poweronState();
                send_request();
                write_bits(id);
                std::cout<<typeid(*this).name()<<":"<<"P"<<std::endl;
                write_byte(static_cast<unsigned char>(id.to_ulong()));
                first_run=false;
            }
            void send_comshutdownRequest() {
                auto id = shutdownState();
                send_request();
                write_bits(id);
                std::cout << typeid(*this).name()<<":"<<"X"<<std::endl;
                write_byte(static_cast<unsigned char>(id.to_ulong()));
                _vars.sent_com_shutdown = true;
            }
            void send_idleRequest() {
                auto id = idleState();
                send_request();
                write_bits(id);
                //std::cout<<typeid(*this).name()<<":"<<"!"<<std::endl;
                write_byte(static_cast<unsigned char>(id.to_ulong()));
            }
            void send_transferRequest(decltype(DMADescriptor::id) unsynced){
                std::bitset<8> id;
                send_request();
                write_bits(id);
                std::bitset<8> obj_id = static_cast<unsigned long>(unsynced); // DANGER: overflow check
                id = id ^ obj_id;
                std::cout<<typeid(*this).name()<<":"<<"T"<<(unsigned int)unsynced<<std::endl;//why not ID?!
                write_byte(static_cast<unsigned char>(id.to_ulong()));
            }
            void send_sighupRequest() {
                auto id = sighupState();
                send_request();
                write_bits(id);
                std::cout << typeid(*this).name()<<":"<<"I"<<std::endl;
                write_byte(static_cast<unsigned char>(id.to_ulong()));
                _vars.sent_sighup = true;
            }
            //acknowledge has priority over request, but requires last read_object byte
            void acknowledge_hook() {
                if (_vars.received_acknowledge)
                {
                    if (!_vars.acknowledgeRequested){
                        SFA::util::logic_error(SFA::util::error_code::AcknowledgeReceivedWithoutAnyRequest, __FILE__, __func__, typeid(*this).name());
                    } else {
                        bool gotOne = false;
                        for (std::size_t j = 0; j < foreign().descriptors.size() && !gotOne; j++){
                            if (j == acknowledgeId){
                                if (foreign().descriptors[j].synced)
                                    SFA::util::logic_error(SFA::util::error_code::ReceivedATransferAcknowledgeOnSyncedObject,__FILE__,__func__, typeid(*this).name());
                                if (foreign().descriptors[j].readLock)
                                    SFA::util::logic_error(SFA::util::error_code::ReceivedATransferAcknowledgeOnReadlockedObject,__FILE__,__func__, typeid(*this).name());
                                if (foreign().descriptors[j].transfer)
                                    SFA::util::logic_error(SFA::util::error_code::ReceivedADuplicateTransferAcknowledgeOnObjectInTransfer,__FILE__,__func__, typeid(*this).name());
                                if (!foreign().descriptors[j].readLock){
                                    foreign().descriptors[j].transfer = true;
                                    std::cout<<typeid(*this).name()<<"."<<"A"<<acknowledgeId<<std::endl;
                                    gotOne = true;
                                } else {
                                    SFA::util::logic_error(SFA::util::error_code::ReadlockPredatesAcknowledge,__FILE__,__func__, typeid(*this).name());
                                }
                            }
                        }
                        if (!gotOne)
                            SFA::util::logic_error(SFA::util::error_code::AcknowledgeIdDoesNotReferenceAValidObject, __FILE__, __func__, typeid(*this).name());
                    }
                } else {
                    if (_vars.acknowledgeRequested){
                        SFA::util::runtime_error(SFA::util::error_code::PreviousTransferHasNotBeenAcknowledged, __FILE__, __func__, typeid(*this).name());
                    }
                }
                acknowledgeId = 255;//overridden at half baud rate
                _vars.acknowledgeRequested = false;//overridden at half baud rate
            }
            void transfer_hook() {
                if (received_request && !(_vars.received_acknowledge && acknowledgeId == requestId)) {
                for (std::size_t j = 0; j < foreign().descriptors.size(); j++) {
                    if (foreign().descriptors[j].id == requestId){
                        if (foreign().descriptors[j].readLock)
                            SFA::util::runtime_error(SFA::util::error_code::DuplicateReadlockRequest,std::to_string(requestId),__func__, typeid(*this).name());
                        if (foreign().descriptors[j].synced)
                        {//acknowledge override case can be omitted: 2 cycles
                            if (!foreign().descriptors[j].transfer){
                                foreign().descriptors[j].readLock = true;
                                std::cout<<typeid(*this).name()<<"."<<"L"<<j<<std::endl;
                                if (receive_lock)
                                    foreign().descriptors[j].queued = true;
                                //acknowledge sets a transfer
                                send_acknowledge();//ALWAYS: use write_bits to set request and acknowledge flags
                            } else {
                                SFA::util::logic_error(SFA::util::error_code::SyncedObjectsAreNotSupposedToHaveaTransfer,__FILE__,__func__, typeid(*this).name());
                            }
                        } else {
                            if (!foreign().descriptors[j].transfer) //OVERRIDE
                                SFA::util::runtime_error(SFA::util::error_code::IncomingReadlockIsCancelingLocalWriteOperation,__FILE__,__func__, typeid(*this).name());
                        }
                    }
                }
                } else {//VALID STATE
                    //SFA::util::logic_error(SFA::util::error_code::IncomingReadlockIsRejectedOrOmitted,__FILE__,__func__, typeid(*this).name());//The other side has to cope with it
                }
                requestId = 255;
            }
            unsigned int writeCount = 0; // write3plus1
            std::size_t writeOrigin = 255;
            std::size_t writeOriginPos = 0;
            unsigned int readCount = 0; // read4minus1
            std::size_t readDestination = 255;
            std::size_t readDestinationPos = 0;
            bool getFirstTransfer()
            {
                for (std::size_t j = 0; j < foreign().descriptors.size(); j++){
                    if (!foreign().descriptors[j].synced && !foreign().descriptors[j].transfer){
                        if (foreign().descriptors[j].readLock)
                            SFA::util::logic_error(SFA::util::error_code::SyncedStatusHasNotBeenOverridenWhenReadlockWasAcquired, __FILE__, __func__, typeid(*this).name());
                        acknowledgeId = j;//overridden when synced is set to false
                        _vars.acknowledgeRequested = true;
                        send_transferRequest(foreign().descriptors[j].id);
                        return true;
                    }
                }
                return false;
            }
            bool getFirstSyncObject()
            {
                for (std::size_t j = 0; j < foreign().descriptors.size(); j++)
                {
                    if (foreign().descriptors[j].readLock && !foreign().descriptors[j].synced)
                        SFA::util::logic_error(SFA::util::error_code::DMAObjectHasEnteredAnIllegalSyncState, __FILE__, __func__, typeid(*this).name());
                    if (foreign().descriptors[j].transfer)
                    {
                        if (foreign().descriptors[j].synced)
                            SFA::util::logic_error(SFA::util::error_code::FoundATransferObjectWhichIsSynced,__FILE__,__func__, typeid(*this).name());
                        if (foreign().descriptors[j].readLock)
                            SFA::util::logic_error(SFA::util::error_code::FoundATransferObjectWhichIsReadlocked,__FILE__,__func__, typeid(*this).name());
                        if (writeOriginPos != 0){
                            SFA::util::logic_error(SFA::util::error_code::PreviousObjectWriteHasNotBeenCompleted,__FILE__,__func__, typeid(*this).name());
                        }
                        send_lock = true;
                        writeOrigin = j;
                        return true;
                    }
                }
                return false;
            }
            bool write_hook(){
                if (first_run){
                    send_poweronRequest();
                    return true;
                }
                if (!send_lock)
                    if (!_vars.sent_com_shutdown? getFirstTransfer() : false) {
                        return true;
                    }
                if (!send_lock)
                    if (incoming_shutdown_query() && !_vars.sent_com_shutdown){
                        send_comshutdownRequest();
                        return true;
                    }
                if (!send_lock)
                    if (getFirstSyncObject())
                        return false;
                if (!send_lock)
                    if (outgoing_sighup_query() && !_vars.sent_sighup){
                        send_sighupRequest();
                        return true;
                    }
                return false;
            }
            bool write_object(int &write3plus1)
            {
                if (send_lock){
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
                        foreign().signal.getAcknowledgeRef().clear();//Used as separate signals, not a handshake
                        foreign().descriptors[writeOrigin].tx_counter++; // DEBUG
                        std::cout<<typeid(*this).name()<<":"<<"W"<<writeOrigin<<std::endl;
                        writeOriginPos = 0;
                    }
                    write3plus1 = 0;
                    write(63); //'?' empty write
                    return true;
                }
                }
                return false;
            }
            void read_object(int &read4minus1, unsigned char &data)
            {
                if (!receive_lock){
                    bool gotOne = false;
                    for (std::size_t j = 0; j < foreign().descriptors.size() && !gotOne; j++){
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
                        foreign().signal.getUpdatedRef().clear();//Used as separate signals, not a handshake
                        foreign().descriptors[readDestination].rx_counter++; // DEBUG
                        std::cout<<typeid(*this).name()<<"."<<"R"<<readDestination<<std::endl;
                        readDestinationPos = 0;
                    }
                    read4minus1 = 0;
                }
                } else {
                    SFA::util::logic_error(SFA::util::error_code::NoIdleReceivedAndNoReceivelockObtained,__FILE__,__func__, typeid(*this).name());
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
