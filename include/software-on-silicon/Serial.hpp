namespace SOS {
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
        bool descendants_notified = false;
    };
}
namespace Behavior {
    class SerialEventSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
        constexpr SerialEventSubController(typename bus_type::signal_type& signal)
        : SubController()
        , _intrinsic(signal)
        {
        }

    protected:
        bus_type::signal_type& _intrinsic;
    };
    class SerialEventDummy : public SOS::Behavior::Loop, protected SOS::Behavior::SerialEventSubController {
    public:
        using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
        SerialEventDummy(typename bus_type::signal_type& signal)
        : Loop()
        , SerialEventSubController(signal)
        {
        }
    };
}
namespace Protocol {
    template <typename... Objects>
    class Serial : protected SOS::Protocol::BlockWiseTransfer<Objects...>, public SOS::Behavior::SerialEventDummy {
    public:
        using bus_type = typename SOS::Protocol::BlockWiseTransfer<Objects...>::bus_type;
        Serial(bus_type& bus, SOS::MemoryView::SerialAsyncBus<Objects...>& other)
            : SOS::Protocol::BlockWiseTransfer<Objects...>(bus, other)
            , SOS::Behavior::SerialEventDummy(bus.signal)
        {
        }
        virtual ~Serial() {}; // request_shutdown_action
        virtual void event_loop() final
        {
            std::this_thread::yield();
            if (handshake_query())
            if (handshake()) {
                // IN
                if (first_run) {
                    SyncProcessor<Objects...>::emit_init();
                } else {
                    unsigned char data = this->read_byte();
                    this->read_bits(data);
                    std::tie(_vars.received_request, _vars.received_acknowledge) = receive_signals();
                    _vars.received_idle = false;
                    incomingRequest = NUM_IDS;
                    if (_vars.received_request) {
                        read_hook(data);
                    } else {
                        this->read_object(data);
                    }
                    transfer_hook(); // requires readLock status from last read byte. if incomingRequest is not the same as the getFirstTransfer we have requested in the previous cycle, we approve the incomingRequest
                    if (_vars.received_acknowledge) {
                        if (!_vars.acknowledgeRequested) {
                            SFA::util::logic_error(SFA::util::error_code::AcknowledgeReceivedWithoutAnyRequest, __FILE__, __func__, typeid(*this).name());
                        } else {
                            acknowledge_hook(); // requires readLock update from transfer_hook
                            _vars.acknowledgeRequested = false;
                        }
                    } else {
                        if (_vars.acknowledgeRequested) {
                            _vars.acknowledgeRequested = false;
                            //clear sync_me
                            //this->bus2.signal[waitingConfirmation].sync_me.test_and_set();
                            SFA::util::runtime_error(SFA::util::error_code::PreviousTransferHasNotBeenAcknowledged, __FILE__, __func__, typeid(*this).name());
                        } else {
                        }
                    }
                }
                // OUT
                waitingConfirmation = NUM_IDS;
                if (!write_hook())
                    if (!this->write_object())
                        send_idleRequest();
                //this->bus2.signal.getNotifyRef().clear(); // was in SerialProcessing
                handshake_ack();
            }
        }

    protected:
        virtual bool handshake()  final
        {
            if (!SOS::Behavior::SerialEventDummy::_intrinsic.getUpdatedRef().test_and_set()) {
                return true;
            }
            return false;
        }
        virtual void handshake_ack() final
        {
            SyncProcessor<Objects...>::trigger_resolve();
            SOS::Behavior::SerialEventDummy::_intrinsic.getAcknowledgeRef().clear();
        }
        virtual void send_acknowledge() = 0; // 3
        virtual void send_request() = 0; // 1
        virtual std::tuple<bool, bool> receive_signals() = 0; // 2 and 4
        virtual void com_hotplug_action() = 0;
        virtual void stop_notifier() final {
            SyncProcessor<Objects...>::emit_interrupted();
            _vars.descendants_notified = true;
        };
        virtual void com_shutdown_action() = 0;
        virtual void com_sighup_action() = 0;
        virtual bool exit_query() = 0;
        virtual bool incoming_shutdown_query() = 0;
        virtual bool outgoing_sighup_query() = 0;
        virtual bool handshake_query() = 0;
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
                if (SyncProcessor<Objects...>::check_sync(j) && !this->descriptors[j].transfer)
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
        unsigned char incomingRequest = NUM_IDS;
        unsigned char waitingConfirmation = NUM_IDS;
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
                    // std::cout << typeid(*this).name() << "." << "!" << std::endl;
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
                incomingRequest = receive_transferRequest(state_code.to_ulong());
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
            // std::cout<<typeid(*this).name() << ":" << "!" << std::endl;
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
                // std::cout << typeid(*this).name() << ":" << "T" << std::to_string(item) << std::endl; // why not ID?!
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
            if (waitingConfirmation < NUM_IDS) {
                if (this->descriptors[waitingConfirmation].readLock)
                    SFA::util::logic_error(SFA::util::error_code::ReceivedATransferAcknowledgeOnReadlockedObject, __FILE__, __func__, typeid(*this).name());
                if (this->descriptors[waitingConfirmation].transfer)
                    SFA::util::logic_error(SFA::util::error_code::ReceivedADuplicateTransferAcknowledgeOnObjectInTransfer, __FILE__, __func__, typeid(*this).name());
                if (!this->descriptors[waitingConfirmation].readLock) { // requires last read_object byte
                    this->descriptors[waitingConfirmation].transfer = true;
                    //this->descriptors[waitingConfirmation].unsynced = false;
                    // std::cout << typeid(*this).name() << "." << "A" << std::to_string(waitingConfirmation) << std::endl;
                    SyncProcessor<Objects...>::emit_transfer(waitingConfirmation);
                } else {
                    SFA::util::logic_error(SFA::util::error_code::ReadlockPredatesAcknowledge, __FILE__, __func__, typeid(*this).name());
                }
            }
        }
        void transfer_hook()
        {
            if (incomingRequest < NUM_IDS) {
                if (incomingRequest != waitingConfirmation) {// start_transfer has priority over send_acknowledge
                    if (this->descriptors[incomingRequest].readLock)
                        SFA::util::runtime_error(SFA::util::error_code::DuplicateReadlockRequest, std::to_string(incomingRequest), __func__, typeid(*this).name());
                    if (!this->descriptors[incomingRequest].transfer) {
                        SyncProcessor<Objects...>::emit_readlocked(incomingRequest);
                        this->descriptors[incomingRequest].readLock = true;
                        // std::cout << typeid(*this).name() << "." << "L" << std::to_string(incomingRequest) << std::endl;
                        send_acknowledge();
                    } else {
                        SFA::util::logic_error(SFA::util::error_code::SyncedObjectsAreNotSupposedToHaveaTransfer, __FILE__, __func__, typeid(*this).name());
                    }
                }
            }

        }
        virtual void emit_received(std::size_t obj_id) final { SyncProcessor<Objects...>::emit_received(obj_id); }
        virtual void emit_sent(std::size_t obj_id) final { SyncProcessor<Objects...>::emit_sent(obj_id); }
        bool getFirstTransfer()
        {
            for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                if (SyncProcessor<Objects...>::check_sync(j) && !this->descriptors[j].transfer) {
                    if (this->descriptors[j].readLock)
                        SFA::util::logic_error(SFA::util::error_code::SyncedStatusHasNotBeenOverridenWhenReadlockWasAcquired, __FILE__, __func__, typeid(*this).name());
                    waitingConfirmation = j;
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
namespace Behavior {
}
}
