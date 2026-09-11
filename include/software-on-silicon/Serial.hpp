namespace SOS {
namespace Behavior {
    template <typename S, typename... Others>
    class SerialPassthruBootstrapEventController : public Controller<S>, public Stoppable, protected StoppableEventSubController {
    public:
        SerialPassthruBootstrapEventController(typename bus_type::signal_type& signal, typename S::bus_type& passThru, Others&... args)
        : Controller<S>()
        , Stoppable()
        , StoppableEventSubController(signal)
        , _foreign(passThru)
        , _child(new S { _foreign, args... })
        {
        }
        ~SerialPassthruBootstrapEventController()
        {
            if (_child) {
                // SFA::util::runtime_error(SFA::util::error_code::ChildHasToBeDeletedBeforeDestroyThread, __FILE__, __func__, typeid(*this).name());
                delete _child;
                _child = nullptr;
            }
        }
        void stop_descendants()
        {
            if (_child) {
                delete _child;
                _child = nullptr;
            } else {
                SFA::util::runtime_error(SFA::util::error_code::ChildHasAlreadyBeenDeleted, __FILE__, __func__, typeid(*this).name());
            }
        }
        bool descendants_stopped() { return !_child; }

    protected:
        typename S::bus_type& _foreign;

    private:
        S* _child = nullptr;
    };
}
namespace Protocol {
    struct com_vars {
        bool received_idle = false;
        bool sent_idle = false;
        bool received_com_shutdown = false;
        bool sent_com_shutdown = false;
        bool received_sighup = false;
        bool sent_sighup = false;
        bool acknowledgeRequested = false;
        bool received_request = false;
        bool received_acknowledge = false;
    };
    template <typename ControllerType, typename... Objects>
    class Serial : protected SOS::Protocol::BlockWiseTransfer<Objects...>, public SOS::Behavior::SerialPassthruBootstrapEventController<ControllerType, SOS::MemoryView::SerialAsyncBus<Objects...>>  {
    public:
        Serial(SOS::MemoryView::DoubleHandShake& signal)
            : SOS::Protocol::BlockWiseTransfer<Objects...>()
            , SOS::Behavior::SerialPassthruBootstrapEventController<ControllerType
            , SOS::MemoryView::SerialAsyncBus<Objects...>>(signal, this->bus3, this->bus2)
        {
        }
        ~Serial()
        {
            std::cout << typeid(*this).name() << " shutdown" << std::endl;
        }
        virtual void event_loop() final
        {
            std::this_thread::yield();
            if (handshake()) {
                // IN
                if (first_run) {
                    emit_init();
                } else {
                    if (aux())
                        request_shutdown_action();
                    unsigned char data = this->read_byte();
                    this->read_bits(data);
                    std::tie(_vars.received_request, _vars.received_acknowledge) = receive_signals();
                    _vars.received_idle = false;
                    if (_vars.received_request) {
                        requestId = NUM_IDS;
                        read_hook(data);
                        if (requestId != NUM_IDS)
                            transfer_hook();
                    } else {
                        this->read_object(data);
                    }
                    if (_vars.received_acknowledge) {
                        if (!_vars.acknowledgeRequested) {
                            SFA::util::logic_error(SFA::util::error_code::AcknowledgeReceivedWithoutAnyRequest, __FILE__, __func__, typeid(*this).name());
                        } else {
                            if (acknowledgeId != NUM_IDS)
                                acknowledge_hook();
                        }
                        _vars.acknowledgeRequested = false;
                    } else {
                        if (_vars.acknowledgeRequested) {
                            //clear sync_me
                            //this->bus2.signal[acknowledgeId].sync_me.test_and_set();
                            SFA::util::runtime_error(SFA::util::error_code::PreviousTransferHasNotBeenAcknowledged, __FILE__, __func__, typeid(*this).name());
                        }
                    }
                }
                // OUT
                acknowledgeId = NUM_IDS;
                if (!write_hook()) // collect_unsynced
                    if (!this->write_object()) // inform_write_end
                        send_idleRequest();
                this->bus2.signal.getNotifyRef().clear();
                if (_vars.sent_sighup)
                    aux_ack();
                handshake_ack();
            }
        }

    protected:
        virtual bool descendants_stopped() final { return SOS::Behavior::SerialPassthruBootstrapEventController<ControllerType, SOS::MemoryView::SerialAsyncBus<Objects...>>::descendants_stopped(); }
        virtual bool handshake()  final
        {
            if (!SOS::Behavior::SerialPassthruBootstrapEventController<ControllerType, SOS::MemoryView::SerialAsyncBus<Objects...>>::_intrinsic.getUpdatedRef().test_and_set()) {
                return true;
            }
            return false;
        }
        virtual void handshake_ack() final
        {
            SOS::Behavior::SerialPassthruBootstrapEventController<ControllerType, SOS::MemoryView::SerialAsyncBus<Objects...>>::_intrinsic.getAcknowledgeRef().clear();
        }
        virtual bool aux() final
        {
            if (!SOS::Behavior::SerialPassthruBootstrapEventController<ControllerType, SOS::MemoryView::SerialAsyncBus<Objects...>>::_intrinsic.getAuxUpdatedRef().test_and_set()) {
                return true;
            }
            return false;
        }
        virtual void aux_ack() final
        {
            SOS::Behavior::SerialPassthruBootstrapEventController<ControllerType, SOS::MemoryView::SerialAsyncBus<Objects...>>::_intrinsic.getAuxAcknowledgeRef().clear();
        }
        virtual void send_acknowledge() = 0; // 3
        virtual void send_request() = 0; // 1
        virtual std::tuple<bool, bool> receive_signals() = 0; // 2 and 4
        virtual void com_hotplug_action() = 0;
        virtual void stop_notifier() final {
            while (this->bus3.signal.getUpdatedRef().test_and_set()) {
                std::cout << ",";
                std::this_thread::yield();
            }
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::serviceinterrupted);
            std::get<0>(this->bus3.cables).getWordRef().store(NUM_IDS);
            this->bus3.signal.getAcknowledgeRef().clear();
            SOS::Behavior::SerialPassthruBootstrapEventController<ControllerType, SOS::MemoryView::SerialAsyncBus<Objects...>>::stop_descendants();
        };
        virtual void request_shutdown_action() = 0;
        virtual void com_shutdown_action() = 0;
        virtual void com_sighup_action() = 0;
        virtual bool exit_query() = 0;
        virtual bool incoming_shutdown_query() = 0;
        virtual bool outgoing_sighup_query() = 0;
        void clear_read_receive()
        {
            if (this->receive_lock || this->readCount != 0) {
                SFA::util::runtime_error(SFA::util::error_code::HotplugAfterUnexpectedShutdown, __FILE__, __func__, typeid(*this).name());
                for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                    if (this->descriptors[j].readLock) {
                        SFA::util::runtime_error(SFA::util::error_code::ObjectCouldBeOutdated, __FILE__, __func__, typeid(*this).name());
                    }
                }
                this->receive_lock = false;
                this->readCount = 0;
                this->readDestinationPos = 0;
            }
        };
        bool transfers_pending()
        {
            for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                if (check_sync(j) && !this->descriptors[j].transfer)
                    return true;
            }
            return false;
        }
        bool writes_pending()
        {
            for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                if (this->descriptors[j].transfer)
                    return true;
            }
            return false;
        }
        bool reads_pending()
        {
            for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                if (this->descriptors[j].readLock)
                    return true;
            }
            return false;
        }
        bool mcu_updated = false; // mcu_write,fpga_read bit 7
        bool fpga_acknowledge = false; // mcu_write,fpga_read bit 6
        bool fpga_updated = false; // mcu_read,fpga_write bit 7
        bool mcu_acknowledge = false; // mcu_read,fpga_write bit 6
        com_vars _vars = com_vars {};

    private:
        bool first_run = true;
        unsigned char requestId = NUM_IDS;
        unsigned char acknowledgeId = NUM_IDS;
        void read_hook(unsigned char& data)
        {
            auto state_code = std::bitset<8> { data };
            state_code <<= NUM_SIGNALBITS;
            state_code >>= NUM_SIGNALBITS;
            if (state_code == std::bitset<8> { state::poweron }) {
                // if (!_vars.received_sighup)
                //     SFA::util::runtime_error(SFA::util::error_code::PreviousCommunicationNotSighupTerminated, __FILE__, __func__, typeid(*this).name());
                if (_vars.acknowledgeRequested || _vars.received_acknowledge)
                    SFA::util::runtime_error(SFA::util::error_code::PreviousTransferRequestsWereNotCleared, __FILE__, __func__, typeid(*this).name());
                _vars = com_vars {};
                com_hotplug_action();
                // start_calc_thread
                // start notifier
                std::cout << typeid(*this).name() << "." << "P" << std::endl;
            } else if (state_code == std::bitset<8> { state::idle }) {
                if (_vars.received_sighup) {
                    _vars.received_idle = true;
                    std::cout << typeid(*this).name() << "." << "!" << std::endl;
                }
            } else if (state_code == std::bitset<8> { state::shutdown }) {
                if (_vars.received_sighup)
                    SFA::util::logic_error(SFA::util::error_code::NotIdleAfterSighup, __FILE__, __func__, typeid(*this).name());
                if (!_vars.received_com_shutdown) {
                    com_shutdown_action();
                    _vars.received_com_shutdown = true;
                    std::cout << typeid(*this).name() << "." << "X" << std::endl;
                } else {
                    SFA::util::logic_error(SFA::util::error_code::DuplicateComShutdown, __FILE__, __func__, typeid(*this).name());
                }
            } else if (state_code == std::bitset<8> { state::sighup }) {
                if (!_vars.received_sighup) {
                    com_sighup_action();
                    _vars.received_sighup = true;
                    std::cout << typeid(*this).name() << "." << "I" << std::endl;
                } else {
                    SFA::util::logic_error(SFA::util::error_code::DuplicateSighup, __FILE__, __func__, typeid(*this).name());
                }
            } else {
                if (_vars.received_sighup)
                    SFA::util::logic_error(SFA::util::error_code::NotIdleAfterSighup, __FILE__, __func__, typeid(*this).name());
                requestId = receive_transferRequest(state_code.to_ulong());
            }
        }
        void send_poweronRequest()
        {
            send_request();
            auto id_bits = std::bitset<8> { state::poweron };
            this->write_bits(id_bits);
            std::cout << typeid(*this).name() << ":" << "P" << std::endl;
            this->write_byte(static_cast<unsigned char>(id_bits.to_ulong()));
            first_run = false;
        }
        void send_comshutdownRequest()
        {
            send_request();
            auto id_bits = std::bitset<8> { state::shutdown };
            this->write_bits(id_bits);
            std::cout << typeid(*this).name() << ":" << "X" << std::endl;
            this->write_byte(static_cast<unsigned char>(id_bits.to_ulong()));
            _vars.sent_com_shutdown = true;
        }
        void send_idleRequest()
        {
            send_request();
            auto id_bits = std::bitset<8> { state::idle };
            this->write_bits(id_bits);
            // std::cout<<typeid(*this).name()<<":"<<"!"<<std::endl;
            this->write_byte(static_cast<unsigned char>(id_bits.to_ulong()));
            if (_vars.sent_sighup)
                _vars.sent_idle = true;
        }
        unsigned char receive_transferRequest(unsigned char mod) { return mod - LOWER_STATES; }
        void send_transferRequest(unsigned char item)
        {
            if (item < NUM_IDS) {
                send_request();
                auto id_bits = std::bitset<8> { 0x00 };
                this->write_bits(id_bits);
                std::cout << typeid(*this).name() << ":" << "T" << std::to_string(item) << std::endl; // why not ID?!
                const unsigned char mod = item + LOWER_STATES;
                auto obj_id = std::bitset<8> { mod };
                id_bits = id_bits ^ obj_id;
                this->write_byte(static_cast<unsigned char>(id_bits.to_ulong()));
            } else {
                SFA::util::runtime_error(SFA::util::error_code::InvalidDMAObjectId, __FILE__, __func__, typeid(*this).name());
            }
        }
        void send_sighupRequest()
        {
            send_request();
            auto id_bits = std::bitset<8> { state::sighup };
            this->write_bits(id_bits);
            std::cout << typeid(*this).name() << ":" << "I" << std::endl;
            this->write_byte(static_cast<unsigned char>(id_bits.to_ulong()));
            _vars.sent_sighup = true;
        }
        void acknowledge_hook()
        {
            if (acknowledgeId != requestId) { // send_acknowledge has priority over start_transfer
                if (this->descriptors[acknowledgeId].readLock)
                    SFA::util::logic_error(SFA::util::error_code::ReceivedATransferAcknowledgeOnReadlockedObject, __FILE__, __func__, typeid(*this).name());
                if (this->descriptors[acknowledgeId].transfer)
                    SFA::util::logic_error(SFA::util::error_code::ReceivedADuplicateTransferAcknowledgeOnObjectInTransfer, __FILE__, __func__, typeid(*this).name());
                if (!this->descriptors[acknowledgeId].readLock) { // requires last read_object byte
                    this->descriptors[acknowledgeId].transfer = true;
                    //this->descriptors[acknowledgeId].unsynced = false;
                    std::cout << typeid(*this).name() << "." << "A" << std::to_string(acknowledgeId) << std::endl;
                    emit_transfer(acknowledgeId);
                    collect_send();
                } else {
                    SFA::util::logic_error(SFA::util::error_code::ReadlockPredatesAcknowledge, __FILE__, __func__, typeid(*this).name());
                }
            }
        }
        void transfer_hook()
        {
            if (!this->descriptors[requestId].readLock)
                if (!this->descriptors[requestId].transfer) {
                    emit_readlocked(requestId);
                    this->descriptors[requestId].readLock = true;
                    std::cout << typeid(*this).name() << "." << "L" << std::to_string(requestId) << std::endl;
                    send_acknowledge();
                } else {
                    SFA::util::logic_error(SFA::util::error_code::SyncedObjectsAreNotSupposedToHaveaTransfer, __FILE__, __func__, typeid(*this).name());
                }
        }
        virtual void collect_request() final
        {
            while (this->bus3.signal.getUpdatedRef().test_and_set())
                std::this_thread::yield();
            auto instruction = std::get<0>(this->bus3.cables).getOpcodeRef().load();
            auto id = std::get<0>(this->bus3.cables).getWordRef().load();
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::nocommand);
            std::get<0>(this->bus3.cables).getWordRef().store(NUM_IDS);
            if (instruction == SOS::Protocol::transferrequest) {
                this->bus3.signal.getUpdatedRef().clear();
                unsigned long i = 0;
                while (i < this->descriptors[id].obj_size) {
                    if (!this->bus3.signal.getUpdatedRef().test_and_set()) {
                        i++;
                        std::get<0>(this->bus3.cables).getWordRef().store(*reinterpret_cast<unsigned char*>(this->descriptors[id].obj)+i);
                        this->bus3.signal.getAcknowledgeRef().clear();
                    }
                    std::this_thread::yield();
                }
            }
        }
        virtual void collect_send() {
            while (this->bus3.signal.getUpdatedRef().test_and_set())
                std::this_thread::yield();
            auto instruction = std::get<0>(this->bus3.cables).getOpcodeRef().load();
            auto id = std::get<0>(this->bus3.cables).getWordRef().load();
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::nocommand);
            std::get<0>(this->bus3.cables).getWordRef().store(NUM_IDS);
            if (instruction == SOS::Protocol::transfersend){
                this->bus3.signal.getUpdatedRef().clear();
                unsigned long i = 0;
                while (i < this->descriptors[id].obj_size) {
                    if (!this->bus3.signal.getUpdatedRef().test_and_set()) {
                        i++;
                        auto tmp = reinterpret_cast<unsigned char*>(this->descriptors[id].obj)+i;
                        *reinterpret_cast<unsigned char*>(tmp) = std::get<0>(this->bus3.cables).getWordRef().load();
                        this->bus3.signal.getAcknowledgeRef().clear();
                    }
                    std::this_thread::yield();
                }
            }
        }
        virtual bool check_sync(std::size_t obj_id) {
            while (this->bus3.signal.getUpdatedRef().test_and_set())
                std::this_thread::yield();
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::checksync);
            std::get<0>(this->bus3.cables).getWordRef().store(obj_id);
            this->bus3.signal.getAcknowledgeRef().clear();
            while (this->bus3.signal.getUpdatedRef().test_and_set())
                std::this_thread::yield();
            auto instruction = std::get<0>(this->bus3.cables).getOpcodeRef().load();
            auto id = std::get<0>(this->bus3.cables).getWordRef().load();
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::nocommand);
            std::get<0>(this->bus3.cables).getWordRef().store(NUM_IDS);
            this->bus3.signal.getUpdatedRef().clear();
            if (instruction == SOS::Protocol::syncresponse && id == obj_id)
                return true;
            return false;
        }
        virtual void emit_init()
        {
            while (this->bus3.signal.getUpdatedRef().test_and_set())
                std::this_thread::yield();
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::init);
            std::get<0>(this->bus3.cables).getWordRef().store(NUM_IDS);
            this->bus3.signal.getAcknowledgeRef().clear();
        }
        virtual void emit_readlocked(std::size_t obj_id)
        {
            while (this->bus3.signal.getUpdatedRef().test_and_set())
                std::this_thread::yield();
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::readstart);
            std::get<0>(this->bus3.cables).getWordRef().store(obj_id);
            this->bus3.signal.getAcknowledgeRef().clear();
        }
        virtual void emit_received(std::size_t obj_id)
        {
            while (this->bus3.signal.getUpdatedRef().test_and_set())
                std::this_thread::yield();
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::readend);
            std::get<0>(this->bus3.cables).getWordRef().store(obj_id);
            this->bus3.signal.getAcknowledgeRef().clear();
        }
        virtual void emit_transfer(std::size_t obj_id)
        {
            while (this->bus3.signal.getUpdatedRef().test_and_set())
                std::this_thread::yield();
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::writestart);
            std::get<0>(this->bus3.cables).getWordRef().store(obj_id);
            this->bus3.signal.getAcknowledgeRef().clear();
        }
        virtual void emit_sent(std::size_t obj_id)
        {
            while (this->bus3.signal.getUpdatedRef().test_and_set())
                std::this_thread::yield();
            std::get<0>(this->bus3.cables).getOpcodeRef().store(SOS::Protocol::writeend);
            std::get<0>(this->bus3.cables).getWordRef().store(obj_id);
            this->bus3.signal.getAcknowledgeRef().clear();
        }
        bool getFirstTransfer()
        {
            for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                if (check_sync(j) && !this->descriptors[j].transfer) {
                    if (this->descriptors[j].readLock)
                        SFA::util::logic_error(SFA::util::error_code::SyncedStatusHasNotBeenOverridenWhenReadlockWasAcquired, __FILE__, __func__, typeid(*this).name());
                    acknowledgeId = j;
                    _vars.acknowledgeRequested = true;
                    send_transferRequest(j);
                    return true;
                }
            }
            return false;
        }
        bool getFirstSyncObject()
        {
            for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                if (this->descriptors[j].transfer) {
                    if (this->descriptors[j].readLock)
                        SFA::util::logic_error(SFA::util::error_code::FoundATransferObjectWhichIsReadlocked, __FILE__, __func__, typeid(*this).name());
                    if (this->writeOriginPos != 0) {
                        SFA::util::logic_error(SFA::util::error_code::PreviousObjectWriteHasNotBeenCompleted, __FILE__, __func__, typeid(*this).name());
                    }
                    this->send_lock = true;
                    this->writeOrigin = j;
                    return true;
                }
            }
            return false;
        }
        bool write_hook()
        {
            if (first_run) {
                send_poweronRequest();
                return true;
            }
            if (!this->send_lock)
                if (!_vars.sent_com_shutdown ? getFirstTransfer() : false) { // unsynced
                    return true;
                }
            if (!this->send_lock)
                if (incoming_shutdown_query() && !_vars.sent_com_shutdown) { // unsynced
                    send_comshutdownRequest();
                    return true;
                }
            if (!this->send_lock)
                if (getFirstSyncObject()) // may check unsynced
                    return false;
            if (!this->send_lock)
                if (outgoing_sighup_query() && !_vars.sent_sighup) {
                    send_sighupRequest();
                    return true;
                }
            return false;
        }
    };
}
}
