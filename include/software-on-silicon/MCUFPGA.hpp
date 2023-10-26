namespace SOS {
    namespace MemoryView{
        class BiDirectionalSignal : public std::array<std::atomic_flag,4> {
            public:
            BiDirectionalSignal() : std::array<std::atomic_flag,4>{true,true,true,true} {}
            auto& getHostOutUpdatedRef(){return std::get<0>(*this);}
            auto& getHostOutAcknowledgeRef(){return std::get<1>(*this);}
            auto& getEmbeddedOutUpdatedRef(){return std::get<2>(*this);}
            auto& getEmbeddedOutAcknowledgeRef(){return std::get<3>(*this);}
        };
        struct bidirectional_tag{};
        struct BusBiDirectional : SOS::MemoryView::bus <
            bidirectional_tag,
            BiDirectionalSignal,
            SOS::MemoryView::bus_traits<SOS::MemoryView::Bus>::cables_type,
            SOS::MemoryView::bus_traits<SOS::MemoryView::Bus>::const_cables_type
        >{
            signal_type signal;
        };
        /*template<typename ArithmeticType> struct HostWrite : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            HostWrite(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
            auto& getHostWriteOffsetRef(){return std::get<0>(*this);}
            auto& getHostWriteLengthRef(){return std::get<1>(*this);}
        };
        template<typename ArithmeticType> struct EmbeddedWrite : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            EmbeddedWrite(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
            auto& getEmbeddedWriteOffsetRef(){return std::get<0>(*this);}
            auto& getEmbeddedWriteLengthRef(){return std::get<1>(*this);}
        };*/
        struct WriteLock : public SOS::MemoryView::BusBiDirectional {//Need BiDirectionalController
            using cables_type = SOS::MemoryView::bus_traits<SOS::MemoryView::Bus>::cables_type;
            //using const_cables_type = std::tuple< HostWrite<char>, EmbeddedWrite<char> >;//0: (char)mcupointer, 1: (char)fpgapointer
            using const_cables_type = SOS::MemoryView::bus_traits<SOS::MemoryView::Bus>::const_cables_type;
        };
    }
    namespace Behavior{
        template<typename S, typename... Others> class BiDirectionalController : public Controller<SOS::MemoryView::BiDirectionalSignal, S> {
            public:
            using bus_type = SOS::MemoryView::WriteLock;
            using subcontroller_type = typename Controller<SOS::MemoryView::BiDirectionalSignal, S>::subcontroller_type;
            BiDirectionalController(typename bus_type::signal_type& signal, Others&... args) :
            Controller<SOS::MemoryView::BiDirectionalSignal, S>(signal),
            _child(subcontroller_type{_foreign, args...})
            {}
            protected:
            typename subcontroller_type::bus_type _foreign = typename subcontroller_type::bus_type{};
            S _child;
        };
        template<> class BiDirectionalController<DummyController> : public Controller<SOS::MemoryView::BiDirectionalSignal, DummyController> {
            public:
            using bus_type = SOS::MemoryView::WriteLock;
            using subcontroller_type = DummyController;
            BiDirectionalController(typename bus_type::signal_type& signal) :
            Controller<SOS::MemoryView::BiDirectionalSignal, DummyController>(signal)
            {}
        };
        template<typename... Objects> class SerialFPGAController :
        public virtual SOS::Protocol::Serial<Objects...>,
        public SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController> {
            public:
            using bus_type = SOS::MemoryView::WriteLock;
            SerialFPGAController(bus_type& myBus) :
            SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>::BiDirectionalController(myBus.signal) {}
            virtual bool handshake_read() final {
                if (!_intrinsic.getHostOutUpdatedRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_read_ack() final {_intrinsic.getHostOutAcknowledgeRef().clear();}
            virtual bool handshake_write() final {
                if (!_intrinsic.getEmbeddedOutAcknowledgeRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_write_ack() final {_intrinsic.getEmbeddedOutUpdatedRef().clear();}
        };
        template<typename FPGAType, typename... Objects> class SerialMCUThread :
        public virtual SOS::Protocol::Serial<Objects...>,
        public Thread<FPGAType> {
            public:
            SerialMCUThread() :
            Thread<FPGAType>() {}
            virtual bool handshake_read() final {
                if (!Thread<FPGAType>::_foreign.signal.getEmbeddedOutUpdatedRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_read_ack() final {Thread<FPGAType>::_foreign.signal.getEmbeddedOutAcknowledgeRef().clear();}
            virtual bool handshake_write() final {
                if (!Thread<FPGAType>::_foreign.signal.getHostOutAcknowledgeRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_write_ack() final {Thread<FPGAType>::_foreign.signal.getHostOutUpdatedRef().clear();}
        };
    }
}