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
        template<typename ProcessingHook, typename DataBus> class SimulationFPGA :
        public virtual SOS::Protocol::Serial<ProcessingHook, DataBus>,
        private SOS::Protocol::SimulationBuffers {
            public:
            SimulationFPGA(const DMA& in_buffer, DMA& out_buffer) :
            SOS::Protocol::SimulationBuffers(in_buffer,out_buffer)
            {
                write_byte(static_cast<unsigned char>(SOS::Protocol::idleState().to_ulong()));//INIT: FPGA initiates communication with an idle byte
                SOS::Behavior::EventController<ProcessingHook, DataBus>::_intrinsic.getAcknowledgeRef().clear();//INIT: start one-way handshake
            }
            private:
            virtual bool handshake() final {
                if (!SOS::Behavior::EventController<ProcessingHook, DataBus>::_intrinsic.getUpdatedRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {SOS::Behavior::EventController<ProcessingHook, DataBus>::_intrinsic.getAcknowledgeRef().clear();}
            virtual unsigned char read_byte() final {
                return SOS::Protocol::SimulationBuffers::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::SimulationBuffers::write_byte(byte);
            }
        };
        template<typename ProcessingHook, typename DataBus> class SimulationMCU :
        public virtual SOS::Protocol::Serial<ProcessingHook, DataBus>,
        private SOS::Protocol::SimulationBuffers {
            public:
            SimulationMCU(const DMA& in_buffer, DMA& out_buffer) :
            SOS::Protocol::SimulationBuffers(in_buffer,out_buffer)
            {}
            private:
            virtual bool handshake() final {
                if (!SOS::Behavior::EventController<ProcessingHook, DataBus>::_intrinsic.getAcknowledgeRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {SOS::Behavior::EventController<ProcessingHook, DataBus>::_intrinsic.getUpdatedRef().clear();}
            virtual unsigned char read_byte() final {
                return SOS::Protocol::SimulationBuffers::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::SimulationBuffers::write_byte(byte);
            }
        };
    }
}