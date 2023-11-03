namespace SOS {
    namespace Behavior{
        template<typename... Objects> class SerialFPGAController :
        public SOS::Protocol::SerialFPGA<Objects...>,
        public SOS::Behavior::EventController<SOS::Behavior::DummyController> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            SerialFPGAController(bus_type& myBus) :
            SOS::Protocol::Serial<Objects...>(),
            SOS::Behavior::EventController<SOS::Behavior::DummyController>::EventController(myBus.signal) {
                out_buffer()[SOS::Protocol::Serial<Objects...>::writePos++]=static_cast<unsigned char>(SOS::Protocol::idleState().to_ulong());//INIT: First byte of com-buffer needs to be valid
                _intrinsic.getAcknowledgeRef().clear();//INIT: start one-way handshake
            }
            virtual bool handshake() final {
                if (!_intrinsic.getUpdatedRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {_intrinsic.getAcknowledgeRef().clear();}
            protected:
            virtual DMA& out_buffer() final {
                return fpga_to_mcu_buffer;
            }
            virtual const DMA& in_buffer() final {
                return mcu_to_fpga_buffer;
            }
        };
        template<typename FPGAType, typename... Objects> class SerialMCUThread :
        public SOS::Protocol::SerialMCU<Objects...>,
        public Thread<FPGAType> {
            public:
            SerialMCUThread() :
            SOS::Protocol::Serial<Objects...>(),
            Thread<FPGAType>() {}
            virtual bool handshake() final {
                if (!Thread<FPGAType>::_foreign.signal.getAcknowledgeRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {Thread<FPGAType>::_foreign.signal.getUpdatedRef().clear();}
            protected:
            virtual DMA& out_buffer() final {
                return mcu_to_fpga_buffer;
            }
            virtual const DMA& in_buffer() final {
                return fpga_to_mcu_buffer;
            }
        };
    }
}