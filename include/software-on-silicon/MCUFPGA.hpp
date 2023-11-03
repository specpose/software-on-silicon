namespace SOS {
    namespace Behavior{
        template<typename... Objects> class SerialFPGAController :
        public virtual SOS::Protocol::Serial<Objects...>,
        public SOS::Behavior::EventController<SOS::Behavior::DummyController> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            SerialFPGAController(bus_type& myBus) :
            SOS::Behavior::EventController<SOS::Behavior::DummyController>::EventController(myBus.signal) {}
            virtual bool handshake() final {
                if (!_intrinsic.getUpdatedRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {_intrinsic.getAcknowledgeRef().clear();}
        };
        template<typename FPGAType, typename... Objects> class SerialMCUThread :
        public virtual SOS::Protocol::Serial<Objects...>,
        public Thread<FPGAType> {
            public:
            SerialMCUThread() :
            Thread<FPGAType>() {}
            virtual bool handshake() final {
                if (!Thread<FPGAType>::_foreign.signal.getAcknowledgeRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {Thread<FPGAType>::_foreign.signal.getUpdatedRef().clear();}
        };
    }
}