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
        template<typename ProcessingHook> class SimulationFPGA :
        public SOS::Protocol::SerialFPGA<ProcessingHook>,
        private SOS::Protocol::SimulationBuffers,
        public SOS::Behavior::EventController<ProcessingHook>,
        public SOS::Behavior::Loop {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            SimulationFPGA(bus_type& myBus, const DMA& in_buffer, DMA& out_buffer) :
            SOS::Protocol::SerialFPGA<ProcessingHook>(),
            SOS::Protocol::Serial<ProcessingHook>(),
            SOS::Protocol::SimulationBuffers(in_buffer,out_buffer),
            SOS::Behavior::EventController<ProcessingHook>(myBus.signal),
            SOS::Behavior::Loop()
            {
                write_byte(static_cast<unsigned char>(SOS::Protocol::idleState().to_ulong()));//INIT: FPGA initiates communication with an idle byte
                SOS::Behavior::EventController<ProcessingHook>::_intrinsic.getAcknowledgeRef().clear();//INIT: start one-way handshake
            }
            //using SOS::Protocol::SerialFPGA<ProcessingHook>::event_loop;
            virtual void event_loop() final {
                SOS::Protocol::Serial<ProcessingHook>::event_loop();
            }
            protected:
            virtual bool isRunning() final {
                if (stop_token.getUpdatedRef().test_and_set()) {
                    return true;
                } else {
                    return false;
                }
            }
            virtual void finished() final {
                stop_token.getAcknowledgeRef().clear();
            }
            virtual constexpr typename ProcessingHook::bus_type& foreign() final {
                return SOS::Behavior::EventController<ProcessingHook>::_foreign;
            }
            private:
            virtual bool handshake() final {
                if (!SOS::Behavior::EventController<ProcessingHook>::_intrinsic.getUpdatedRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {SOS::Behavior::EventController<ProcessingHook>::_intrinsic.getAcknowledgeRef().clear();}
            virtual unsigned char read_byte() final {
                return SOS::Protocol::SimulationBuffers::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::SimulationBuffers::write_byte(byte);
            }
        };
        template<typename ProcessingHook> class SimulationMCU :
        public SOS::Protocol::SerialMCU<ProcessingHook>,
        private SOS::Protocol::SimulationBuffers,
        public SOS::Behavior::EventController<ProcessingHook>,
        public SOS::Behavior::Loop {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            SimulationMCU(bus_type& myBus, const DMA& in_buffer, DMA& out_buffer) :
            SOS::Protocol::SerialMCU<ProcessingHook>(),
            SOS::Protocol::Serial<ProcessingHook>(),
            SOS::Protocol::SimulationBuffers(in_buffer,out_buffer),
            SOS::Behavior::EventController<ProcessingHook>(myBus.signal),
            SOS::Behavior::Loop()
            {}
            //using SOS::Protocol::Serial<ProcessingHook>::event_loop;
            virtual void event_loop() final {
                SOS::Protocol::Serial<ProcessingHook>::event_loop();
            }
            protected:
            virtual bool isRunning() final {
                if (stop_token.getUpdatedRef().test_and_set()) {
                    return true;
                } else {
                    return false;
                }
            }
            virtual void finished() final {
                stop_token.getAcknowledgeRef().clear();
            }
            virtual constexpr typename ProcessingHook::bus_type& foreign() final {
                return SOS::Behavior::EventController<ProcessingHook>::_foreign;
            }
            private:
            virtual bool handshake() final {
                if (!SOS::Behavior::EventController<ProcessingHook>::_intrinsic.getAcknowledgeRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {SOS::Behavior::EventController<ProcessingHook>::_intrinsic.getUpdatedRef().clear();}
            virtual unsigned char read_byte() final {
                return SOS::Protocol::SimulationBuffers::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::SimulationBuffers::write_byte(byte);
            }
        };
    }
}