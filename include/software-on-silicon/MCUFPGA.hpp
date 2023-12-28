namespace SOS {
    namespace Protocol {
        class SimulationBuffers {
            public:
            SimulationBuffers(const COM_BUFFER& in_buffer, COM_BUFFER& out_buffer) : in_buffer(in_buffer), out_buffer(out_buffer) {}
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
            const COM_BUFFER& in_buffer;
            COM_BUFFER& out_buffer;
        };
    }
    namespace Behavior{
        template<typename ControllerType, typename... Objects> class SimulationFPGA :
        public SOS::Protocol::SerialFPGA<Objects...>,
        private SOS::Protocol::SimulationBuffers,
        public SOS::Behavior::EventController<ControllerType> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            SimulationFPGA(bus_type& myBus, const COM_BUFFER& in_buffer, COM_BUFFER& out_buffer) :
            SOS::Protocol::SerialFPGA<Objects...>(),
            SOS::Protocol::Serial<Objects...>(),
            SOS::Protocol::SimulationBuffers(in_buffer,out_buffer),
            SOS::Behavior::EventController<ControllerType>(myBus.signal)
            {
                write_byte(static_cast<unsigned char>(SOS::Protocol::idleState().to_ulong()));//INIT: FPGA initiates communication with an idle byte
                SOS::Behavior::EventController<ControllerType>::_intrinsic.getAcknowledgeRef().clear();//INIT: start one-way handshake
            }
            //using SOS::Protocol::SerialFPGA<Objects...>::event_loop;
            virtual void event_loop() final {
                SOS::Protocol::Serial<Objects...>::event_loop();
            }
            protected:
            virtual bool isRunning() final {
                if (SOS::Behavior::EventController<ControllerType>::stop_token.getUpdatedRef().test_and_set()) {
                    return true;
                } else {
                    return false;
                }
            }
            virtual void finished() final {
                SOS::Behavior::EventController<ControllerType>::stop_token.getAcknowledgeRef().clear();
            }
            virtual constexpr typename SOS::MemoryView::SerialProcessNotifier<Objects...>& foreign() final {
                return SOS::Behavior::EventController<ControllerType>::_foreign;
            }
            private:
            virtual bool handshake() final {
                if (!SOS::Behavior::EventController<ControllerType>::_intrinsic.getUpdatedRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {SOS::Behavior::EventController<ControllerType>::_intrinsic.getAcknowledgeRef().clear();}
            virtual unsigned char read_byte() final {
                return SOS::Protocol::SimulationBuffers::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::SimulationBuffers::write_byte(byte);
            }
        };
        template<typename ControllerType, typename... Objects> class SimulationMCU :
        public SOS::Protocol::SerialMCU<Objects...>,
        private SOS::Protocol::SimulationBuffers,
        public SOS::Behavior::EventController<ControllerType> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            SimulationMCU(bus_type& myBus, const COM_BUFFER& in_buffer, COM_BUFFER& out_buffer) :
            SOS::Protocol::SerialMCU<Objects...>(),
            SOS::Protocol::Serial<Objects...>(),
            SOS::Protocol::SimulationBuffers(in_buffer,out_buffer),
            SOS::Behavior::EventController<ControllerType>(myBus.signal)
            {}
            //using SOS::Protocol::Serial<Objects...>::event_loop;
            virtual void event_loop() final {
                SOS::Protocol::Serial<Objects...>::event_loop();
            }
            protected:
            virtual bool isRunning() final {
                if (SOS::Behavior::EventController<ControllerType>::stop_token.getUpdatedRef().test_and_set()) {
                    return true;
                } else {
                    return false;
                }
            }
            virtual void finished() final {
                SOS::Behavior::EventController<ControllerType>::stop_token.getAcknowledgeRef().clear();
            }
            virtual constexpr typename SOS::MemoryView::SerialProcessNotifier<Objects...>& foreign() final {
                return SOS::Behavior::EventController<ControllerType>::_foreign;
            }
            private:
            virtual bool handshake() final {
                if (!SOS::Behavior::EventController<ControllerType>::_intrinsic.getAcknowledgeRef().test_and_set()){
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final {SOS::Behavior::EventController<ControllerType>::_intrinsic.getUpdatedRef().clear();}
            virtual unsigned char read_byte() final {
                return SOS::Protocol::SimulationBuffers::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::SimulationBuffers::write_byte(byte);
            }
        };
    }
}