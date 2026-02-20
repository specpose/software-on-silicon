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
    };
    template <typename... Objects>
    class Serial : protected SOS::Protocol::BlockWiseTransfer<Objects...> {
    public:
        Serial(SOS::MemoryView::SerialProcessNotifier<Objects...>& bus) : bus(bus), SOS::Protocol::BlockWiseTransfer<Objects...>(bus.objects) {}
        ~Serial()
        {
            std::cout << typeid(*this).name() << " shutdown" << std::endl;
        }
        virtual void event_loop()
        { // final
            std::this_thread::yield();
            if (handshake()) {
                // IN
                if (!first_run) {
                    if (aux())
                        request_shutdown_action();
                    unsigned char data = this->read_byte();
                    this->read_bits(data);
                    std::tie(_vars.received_request, _vars.received_acknowledge) = receive_signals();
                    _vars.received_idle = false;
                    if (_vars.received_request)
                        read_hook(data);
                    else
                        this->read_object(data);//inform_read_end
                    transfer_hook();//inform_read_start; may check unsynced
                    acknowledge_hook();//inform_write_start; sets unsynced
                }
                collect_sync();
                // OUT
                if (!write_hook())//collect_unsynced
                    if (!this->write_object())//inform_write_end
                        send_idleRequest();
                if (_vars.sent_sighup)
                    aux_ack();
                handshake_ack();
            }
        }

    protected:
        virtual bool descendants_stopped() = 0;
        virtual bool handshake() = 0;
        virtual void handshake_ack() = 0;
        virtual bool aux() = 0;
        virtual void aux_ack() = 0;
        virtual void send_acknowledge() = 0; // 3
        virtual void send_request() = 0; // 1
        virtual std::tuple<bool, bool> receive_signals() = 0; // 2 and 4
        virtual void com_hotplug_action() = 0;
        virtual void stop_notifier() = 0;
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
                if (this->descriptors[j].unsynced && !this->descriptors[j].transfer)
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
        SOS::MemoryView::SerialProcessNotifier<Objects...>& bus;
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
        void send_transferRequest(decltype(DMADescriptor::id) item)
        {
            if (item < NUM_IDS) {
                send_request();
                auto id_bits = std::bitset<8> { 0x00 };
                this->write_bits(id_bits);
                //std::cout << typeid(*this).name() << ":" << "T" << std::to_string(item) << std::endl; // why not ID?!
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
        // acknowledge has priority over request, but requires last read_object byte
        void acknowledge_hook()
        {
            if (_vars.received_acknowledge) {
                if (!_vars.acknowledgeRequested) {
                    SFA::util::logic_error(SFA::util::error_code::AcknowledgeReceivedWithoutAnyRequest, __FILE__, __func__, typeid(*this).name());
                } else {
                    bool gotOne = false;
                    for (unsigned char j = 0; j < this->descriptors.size() && !gotOne; j++) {
                        if (j == acknowledgeId) {
                            if (!this->descriptors[j].unsynced)
                                SFA::util::logic_error(SFA::util::error_code::ReceivedATransferAcknowledgeOnSyncedObject, __FILE__, __func__, typeid(*this).name());
                            if (this->descriptors[j].readLock)
                                SFA::util::logic_error(SFA::util::error_code::ReceivedATransferAcknowledgeOnReadlockedObject, __FILE__, __func__, typeid(*this).name());
                            if (this->descriptors[j].transfer)
                                SFA::util::logic_error(SFA::util::error_code::ReceivedADuplicateTransferAcknowledgeOnObjectInTransfer, __FILE__, __func__, typeid(*this).name());
                            if (!this->descriptors[j].readLock) {
                                this->descriptors[j].transfer = true;
                                this->descriptors[j].unsynced = false;
                                emit_sync_canceled(j);
                                //std::cout << typeid(*this).name() << "." << "A" << std::to_string(acknowledgeId) << std::endl;
                                gotOne = true;
                            } else {
                                SFA::util::logic_error(SFA::util::error_code::ReadlockPredatesAcknowledge, __FILE__, __func__, typeid(*this).name());
                            }
                        }
                    }
                    if (!gotOne)
                        SFA::util::logic_error(SFA::util::error_code::AcknowledgeIdDoesNotReferenceAValidObject, __FILE__, __func__, typeid(*this).name());
                }
            } else {
                if (_vars.acknowledgeRequested) {
                    SFA::util::runtime_error(SFA::util::error_code::PreviousTransferHasNotBeenAcknowledged, __FILE__, __func__, typeid(*this).name());
                }
            }
            acknowledgeId = NUM_IDS;
            _vars.acknowledgeRequested = false;
        }
        void transfer_hook()
        {
            if (_vars.received_request && !(_vars.received_acknowledge && acknowledgeId == requestId)) {
                for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                    if (this->descriptors[j].id == requestId) {
                        if (this->descriptors[j].readLock)
                            SFA::util::runtime_error(SFA::util::error_code::DuplicateReadlockRequest, std::to_string(requestId), __func__, typeid(*this).name());
                        if (!this->descriptors[j].unsynced) {
                            if (!this->descriptors[j].transfer) {
                                this->descriptors[j].readLock = true;
                                //std::cout << typeid(*this).name() << "." << "L" << std::to_string(j) << std::endl;
                                send_acknowledge(); // ALWAYS: use write_bits to set request and acknowledge flags
                            } else {
                                SFA::util::logic_error(SFA::util::error_code::SyncedObjectsAreNotSupposedToHaveaTransfer, __FILE__, __func__, typeid(*this).name());
                            }
                        } else {
                            if (!this->descriptors[j].transfer) // OVERRIDE
                                SFA::util::runtime_error(SFA::util::error_code::IncomingReadlockIsCancelingLocalWriteOperation, __FILE__, __func__, typeid(*this).name());
                        }
                    }
                }
            } else { // VALID STATE
                // SFA::util::logic_error(SFA::util::error_code::IncomingReadlockIsRejectedOrOmitted,__FILE__,__func__, typeid(*this).name());//The other side has to cope with it
            }
            requestId = NUM_IDS;
        }
        void collect_sync() {
            if (!bus.signal.getSyncStartAcknowledgeRef().test_and_set()) {
                const auto id = bus.syncStartId().load();
                this->descriptors[id].unsynced = true;
                bus.signal.getSyncStartUpdatedRef().clear();
            }
        }
        void emit_sync_canceled(std::size_t obj_id){
            while (bus.signal.getSyncStopUpdatedRef().test_and_set())
                std::this_thread::yield();
            bus.syncStopId().store(obj_id);
            bus.signal.getSyncStopAcknowledgeRef().clear();
        }
        virtual void emit_received(std::size_t obj_id) {
            while (bus.signal.getReadUpdatedRef().test_and_set())
                std::this_thread::yield();
            bus.receiveNotificationId().store(obj_id);
            bus.signal.getReadAcknowledgeRef().clear();
        }
        virtual void emit_sent(std::size_t obj_id) {
            while (bus.signal.getWriteUpdatedRef().test_and_set())
                std::this_thread::yield();
            bus.sendNotificationId().store(obj_id);
            bus.signal.getWriteAcknowledgeRef().clear();
        }
        bool getFirstTransfer()
        {
            for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                if (this->descriptors[j].unsynced && !this->descriptors[j].transfer) {
                    if (this->descriptors[j].readLock)
                        SFA::util::logic_error(SFA::util::error_code::SyncedStatusHasNotBeenOverridenWhenReadlockWasAcquired, __FILE__, __func__, typeid(*this).name());
                    acknowledgeId = j;
                    _vars.acknowledgeRequested = true;
                    send_transferRequest(this->descriptors[j].id);
                    return true;
                }
            }
            return false;
        }
        bool getFirstSyncObject()
        {
            for (unsigned char j = 0; j < this->descriptors.size(); j++) {
                if (this->descriptors[j].readLock && this->descriptors[j].unsynced)
                    SFA::util::logic_error(SFA::util::error_code::DMAObjectHasEnteredAnIllegalSyncState, __FILE__, __func__, typeid(*this).name());
                if (this->descriptors[j].transfer) {
                    if (this->descriptors[j].unsynced)
                        SFA::util::logic_error(SFA::util::error_code::FoundATransferObjectWhichIsUnsynced, __FILE__, __func__, typeid(*this).name());
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
                if (!_vars.sent_com_shutdown ? getFirstTransfer() : false) {//unsynced
                    return true;
                }
            if (!this->send_lock)
                if (incoming_shutdown_query() && !_vars.sent_com_shutdown) {//unsynced
                    send_comshutdownRequest();
                    return true;
                }
            if (!this->send_lock)
                if (getFirstSyncObject())//may check unsynced
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
