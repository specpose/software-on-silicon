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
        bool received_acknowledge = false;
    };
    template <typename... Objects>
    class Serial : protected SOS::Protocol::BlockWiseTransfer<Objects...> {
    public:
        Serial()
            : SOS::Protocol::BlockWiseTransfer<Objects...> {}
        {
        }
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
                    _vars.received_acknowledge = receive_acknowledge();
                    received_request = receive_request();
                    _vars.received_idle = false;
                    if (received_request)
                        read_hook(data);
                    else
                        this->read_object(data);//inform_read_end
                    transfer_hook();//inform_read_start, can cancel write_start; may check unsynced
                    acknowledge_hook();//inform_write_start; may check unsynced
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
        virtual bool receive_request() = 0; // 2
        virtual bool receive_acknowledge() = 0; // 4
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
                for (unsigned char j = 0; j < this->foreign().descriptors.size(); j++) {
                    if (this->foreign().descriptors[j].readLock) {
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
            for (unsigned char j = 0; j < this->foreign().descriptors.size(); j++) {
                if (this->foreign().descriptors[j].unsynced && !this->foreign().descriptors[j].transfer)
                    return true;
            }
            return false;
        }
        bool writes_pending()
        {
            for (unsigned char j = 0; j < this->foreign().descriptors.size(); j++) {
                if (this->foreign().descriptors[j].transfer)
                    return true;
            }
            return false;
        }
        bool reads_pending()
        {
            for (unsigned char j = 0; j < this->foreign().descriptors.size(); j++) {
                if (this->foreign().descriptors[j].readLock)
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
        bool received_request = false;
        unsigned char requestId = NUM_IDS;
        unsigned char acknowledgeId = NUM_IDS;
        void read_hook(unsigned char& data)
        {
            auto state_code = (std::bitset<8> { data } << NUM_SIGNALBITS) >> NUM_SIGNALBITS;
            if (state_code == ((std::bitset<8> { state::poweron } << NUM_SIGNALBITS) >> NUM_SIGNALBITS)) {
                // if (!_vars.received_sighup)
                //     SFA::util::runtime_error(SFA::util::error_code::PreviousCommunicationNotSighupTerminated, __FILE__, __func__, typeid(*this).name());
                if (_vars.acknowledgeRequested || _vars.received_acknowledge)
                    SFA::util::runtime_error(SFA::util::error_code::PreviousTransferRequestsWereNotCleared, __FILE__, __func__, typeid(*this).name());
                _vars = com_vars {};
                com_hotplug_action();
                // start_calc_thread
                // start notifier
            } else if (state_code == ((std::bitset<8> { state::idle } << NUM_SIGNALBITS) >> NUM_SIGNALBITS)) {
                if (_vars.received_sighup) {
                    _vars.received_idle = true;
                    std::cout << typeid(*this).name() << "." << "!" << std::endl;
                }
            } else if (state_code == ((std::bitset<8> { state::shutdown } << NUM_SIGNALBITS) >> NUM_SIGNALBITS)) {
                if (_vars.received_sighup)
                    SFA::util::logic_error(SFA::util::error_code::NotIdleAfterSighup, __FILE__, __func__, typeid(*this).name());
                if (!_vars.received_com_shutdown) {
                    com_shutdown_action();
                    _vars.received_com_shutdown = true;
                    std::cout << typeid(*this).name() << "." << "X" << std::endl;
                } else {
                    SFA::util::logic_error(SFA::util::error_code::DuplicateComShutdown, __FILE__, __func__, typeid(*this).name());
                }
            } else if (state_code == ((std::bitset<8> { state::sighup } << NUM_SIGNALBITS) >> NUM_SIGNALBITS)) {
                if (!_vars.received_sighup) {
                    com_sighup_action();
                    _vars.received_sighup = true;
                } else {
                    SFA::util::logic_error(SFA::util::error_code::DuplicateSighup, __FILE__, __func__, typeid(*this).name());
                }
            } else {
                if (_vars.received_sighup)
                    SFA::util::logic_error(SFA::util::error_code::NotIdleAfterSighup, __FILE__, __func__, typeid(*this).name());
                requestId = state_code.to_ulong() - NUM_STATES;
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
        void send_transferRequest(decltype(DMADescriptor::id) item)
        {
            if (item < NUM_IDS) {
                send_request();
                auto id_bits = std::bitset<8> { 0x00 };
                this->write_bits(id_bits);
                auto obj_id = std::bitset<8> { item }; // DANGER: overflow check
                id_bits = id_bits ^ obj_id;
                std::cout << typeid(*this).name() << ":" << "T" << std::to_string(item - NUM_STATES) << std::endl; // why not ID?!
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
                    for (unsigned char j = 0; j < this->foreign().descriptors.size() && !gotOne; j++) {
                        if (j == acknowledgeId) {
                            if (!this->foreign().descriptors[j].unsynced)
                                SFA::util::logic_error(SFA::util::error_code::ReceivedATransferAcknowledgeOnSyncedObject, __FILE__, __func__, typeid(*this).name());
                            if (this->foreign().descriptors[j].readLock)
                                SFA::util::logic_error(SFA::util::error_code::ReceivedATransferAcknowledgeOnReadlockedObject, __FILE__, __func__, typeid(*this).name());
                            if (this->foreign().descriptors[j].transfer)
                                SFA::util::logic_error(SFA::util::error_code::ReceivedADuplicateTransferAcknowledgeOnObjectInTransfer, __FILE__, __func__, typeid(*this).name());
                            if (!this->foreign().descriptors[j].readLock) {
                                this->foreign().descriptors[j].transfer = true;
                                std::cout << typeid(*this).name() << "." << "A" << std::to_string(acknowledgeId) << std::endl;
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
            if (received_request && !(_vars.received_acknowledge && acknowledgeId == requestId)) {
                for (unsigned char j = 0; j < this->foreign().descriptors.size(); j++) {
                    if (this->foreign().descriptors[j].id == requestId) {
                        if (this->foreign().descriptors[j].readLock)
                            SFA::util::runtime_error(SFA::util::error_code::DuplicateReadlockRequest, std::to_string(requestId), __func__, typeid(*this).name());
                        if (!this->foreign().descriptors[j].unsynced) {
                            if (!this->foreign().descriptors[j].transfer) {
                                this->foreign().descriptors[j].readLock = true;
                                std::cout << typeid(*this).name() << "." << "L" << std::to_string(j) << std::endl;
                                send_acknowledge(); // ALWAYS: use write_bits to set request and acknowledge flags
                            } else {
                                SFA::util::logic_error(SFA::util::error_code::SyncedObjectsAreNotSupposedToHaveaTransfer, __FILE__, __func__, typeid(*this).name());
                            }
                        } else {
                            if (!this->foreign().descriptors[j].transfer) // OVERRIDE
                                SFA::util::runtime_error(SFA::util::error_code::IncomingReadlockIsCancelingLocalWriteOperation, __FILE__, __func__, typeid(*this).name());
                        }
                    }
                }
            } else { // VALID STATE
                // SFA::util::logic_error(SFA::util::error_code::IncomingReadlockIsRejectedOrOmitted,__FILE__,__func__, typeid(*this).name());//The other side has to cope with it
            }
            requestId = NUM_IDS;
        }
        void collect_sync()
        {
            if (!this->foreign().signal.getSyncUpdatedRef().test_and_set()){
                while (this->foreign().signal.getSyncAcknowledgeRef().test_and_set())
                    std::this_thread::yield();
                const auto id = this->foreign().sendNotificationId().load();
                this->foreign().descriptors[id].unsynced=true;
            }
        }
        bool getFirstTransfer()
        {
            for (unsigned char j = 0; j < this->foreign().descriptors.size(); j++) {
                if (this->foreign().descriptors[j].unsynced && !this->foreign().descriptors[j].transfer) {
                    if (this->foreign().descriptors[j].readLock)
                        SFA::util::logic_error(SFA::util::error_code::SyncedStatusHasNotBeenOverridenWhenReadlockWasAcquired, __FILE__, __func__, typeid(*this).name());
                    acknowledgeId = j;
                    _vars.acknowledgeRequested = true;
                    send_transferRequest(this->foreign().descriptors[j].id + NUM_STATES);
                    return true;
                }
            }
            return false;
        }
        bool getFirstSyncObject()
        {
            for (unsigned char j = 0; j < this->foreign().descriptors.size(); j++) {
                if (this->foreign().descriptors[j].readLock && this->foreign().descriptors[j].unsynced)
                    SFA::util::logic_error(SFA::util::error_code::DMAObjectHasEnteredAnIllegalSyncState, __FILE__, __func__, typeid(*this).name());
                if (this->foreign().descriptors[j].transfer) {
                    if (!this->foreign().descriptors[j].unsynced)
                        SFA::util::logic_error(SFA::util::error_code::FoundATransferObjectWhichIsSynced, __FILE__, __func__, typeid(*this).name());
                    if (this->foreign().descriptors[j].readLock)
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
    template <typename... Objects>
    class SerialFPGA : protected virtual Serial<Objects...> {
    public:
        SerialFPGA() { }

    private:
        virtual void read_bits(std::bitset<8> temp) final
        {
            Serial<Objects...>::mcu_updated = !temp[7];
            Serial<Objects...>::fpga_acknowledge = temp[6];
            Serial<Objects...>::mcu_acknowledge = false;
        }
        virtual void write_bits(std::bitset<8>& out) final
        {
            if (Serial<Objects...>::fpga_updated)
                out.set(7, 0);
            else
                out.set(7, 1);
            if (Serial<Objects...>::mcu_acknowledge)
                out.set(6, 1);
            else
                out.set(6, 0);
        }
        virtual void send_acknowledge() final
        {
            if (Serial<Objects...>::mcu_updated) {
                Serial<Objects...>::mcu_acknowledge = true;
            }
        }
        virtual void send_request() final
        {
            Serial<Objects...>::fpga_updated = true;
        }
        virtual bool receive_acknowledge() final
        {
            if (Serial<Objects...>::fpga_acknowledge) {
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
    class SerialMCU : protected virtual Serial<Objects...> {
    public:
        SerialMCU() { }

    private:
        virtual void read_bits(std::bitset<8> temp) final
        {
            Serial<Objects...>::fpga_updated = !temp[7];
            Serial<Objects...>::mcu_acknowledge = temp[6];
            Serial<Objects...>::fpga_acknowledge = false;
        }
        virtual void write_bits(std::bitset<8>& out) final
        {
            if (Serial<Objects...>::mcu_updated)
                out.set(7, 0);
            else
                out.set(7, 1);
            if (Serial<Objects...>::fpga_acknowledge)
                out.set(6, 1);
            else
                out.set(6, 0);
        }
        virtual void send_acknowledge() final
        {
            if (Serial<Objects...>::fpga_updated) {
                Serial<Objects...>::fpga_acknowledge = true;
            }
        }
        virtual void send_request() final
        {
            Serial<Objects...>::mcu_updated = true;
        }
        virtual bool receive_acknowledge() final
        {
            if (Serial<Objects...>::mcu_acknowledge) {
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
