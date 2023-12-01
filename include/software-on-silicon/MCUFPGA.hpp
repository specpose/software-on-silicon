namespace SOS {
    namespace Protocol {
        class SimulationBuffers {
            public:
            SimulationBuffers(const DMA& in_buffer, DMA& out_buffer) : in_buffer(in_buffer), out_buffer(out_buffer) {}
            protected:
            virtual unsigned char read_byte() {
                auto byte = in_buffer[readPos++];
                if (readPos==in_buffer.size())
                    readPos=0;
                return byte;
            }
            virtual void write_byte(unsigned char byte) {
                out_buffer[writePos++]=byte;
                if (writePos==out_buffer.size())
                    writePos=0;
            }
            private:
            std::size_t writePos = 0;//REPLACE: out_buffer
            std::size_t readPos = 0;//REPLACE: in_buffer
            const DMA& in_buffer;
            DMA& out_buffer;
        };
    }
    namespace Behavior{
        template<typename... Objects> class SerialFPGAController :
        SOS::Protocol::SimulationBuffers,
        public SOS::Protocol::SerialFPGA<Objects...>,
        public SOS::Behavior::EventController<SOS::Behavior::DummyController> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            SerialFPGAController(bus_type& myBus,const DMA& in_buffer, DMA& out_buffer) :
            SOS::Protocol::SimulationBuffers(in_buffer,out_buffer),
            SOS::Protocol::Serial<Objects...>(),
            SOS::Behavior::EventController<SOS::Behavior::DummyController>::EventController(myBus.signal) {
                write_byte(static_cast<unsigned char>(SOS::Protocol::idleState().to_ulong()));//INIT: FPGA initiates communication with an idle byte
                _intrinsic.getAcknowledgeRef().clear();//INIT: start one-way handshake
            }
            virtual bool handshake() final {
                if (!_intrinsic.getUpdatedRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {_intrinsic.getAcknowledgeRef().clear();}
            virtual unsigned char read_byte() final {
                return SOS::Protocol::SimulationBuffers::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::SimulationBuffers::write_byte(byte);
            }
        };
        template<typename FPGAType, typename... Objects> class SerialMCUThread :
        SOS::Protocol::SimulationBuffers,
        public SOS::Protocol::SerialMCU<Objects...>,
        public Thread<FPGAType> {
            public:
            SerialMCUThread(const DMA& in_buffer, DMA& out_buffer) :
            SOS::Protocol::SimulationBuffers(in_buffer,out_buffer),
            SOS::Protocol::Serial<Objects...>(),
            Thread<FPGAType>() {}
            virtual bool handshake() final {
                if (!Thread<FPGAType>::_foreign.signal.getAcknowledgeRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {Thread<FPGAType>::_foreign.signal.getUpdatedRef().clear();}
            virtual unsigned char read_byte() final {
                return SOS::Protocol::SimulationBuffers::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::SimulationBuffers::write_byte(byte);
            }
        };
    }
}