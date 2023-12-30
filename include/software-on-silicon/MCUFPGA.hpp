namespace SOS {
    namespace MemoryView {
        template<typename ComIterator> struct ComSize : private SOS::MemoryView::ConstCable<ComIterator,4> {
            ComSize(const ComIterator InStart, const ComIterator InEnd, const ComIterator OutStart, const ComIterator OutEnd) :
            SOS::MemoryView::ConstCable<ComIterator,4>{InStart,InEnd,OutStart,OutEnd} {}
            auto& getInBufferStartRef(){return std::get<0>(*this);}
            auto& getInBufferEndRef(){return std::get<1>(*this);}
            auto& getOutBufferStartRef(){return std::get<2>(*this);}
            auto& getOutBufferEndRef(){return std::get<3>(*this);}
        };
        template<typename ComIterator> struct ComOffset : private SOS::MemoryView::TaskCable<ComIterator,2> {
            ComOffset() : SOS::MemoryView::TaskCable<ComIterator,2>{0,0} {}
            auto& getReadOffsetRef(){return std::get<0>(*this);}
            auto& getWriteOffsetRef(){return std::get<1>(*this);}
        };
        template<typename ComBufferType> struct ComBus : public SOS::MemoryView::BusShaker {
            using const_cables_type = std::tuple< ComSize<typename ComBufferType::iterator> >;
            using cables_type = std::tuple< ComOffset<typename ComBufferType::difference_type> >;
            ComBus(const typename ComBufferType::iterator& inStart, const typename ComBufferType::iterator& inEnd,const typename ComBufferType::iterator& outStart, const typename ComBufferType::iterator& outEnd):
            const_cables(
            std::tuple< ComSize<typename ComBufferType::iterator> >{ComSize<typename ComBufferType::iterator>(inStart,inEnd,outStart,outEnd)}
            ) {
                if(std::distance(inStart,inEnd)<1)
                    throw SFA::util::logic_error("ComBuffer size is minimum WORD_SIZE.",__FILE__,__func__);
                if(std::distance(outStart,outEnd)<1)
                    throw SFA::util::logic_error("ComBuffer size is minimum WORD_SIZE.",__FILE__,__func__);
                if(std::distance(inStart,inEnd)!=std::distance(outStart,outEnd))
                    throw SFA::util::logic_error("ComBuffer in and out size not equal.",__FILE__,__func__);
                }
            cables_type cables;
            const_cables_type const_cables;
        };
    }
    namespace Protocol {
        template<typename ComBufferType> class SimulationBuffers {
            public:
            using buffers_ct = typename std::tuple_element<0,typename SOS::MemoryView::ComBus<ComBufferType>::const_cables_type>::type;
            using iterators_ct = typename std::tuple_element<0,typename SOS::MemoryView::ComBus<ComBufferType>::cables_type>::type;
            SimulationBuffers(buffers_ct& com, iterators_ct& it) : comcable(com), itercable(it) {}
            protected:
            virtual unsigned char read_byte() = 0;
            virtual void write_byte(unsigned char byte) = 0;
            std::size_t writePos = 0;//REPLACE: out_buffer
            std::size_t readPos = 0;//REPLACE: in_buffer
            buffers_ct& comcable;
            iterators_ct& itercable;
        };
        template<typename ComBufferType> class FPGASimulationBuffers : SimulationBuffers<ComBufferType> {
            public:
            FPGASimulationBuffers(typename SimulationBuffers<ComBufferType>::buffers_ct& com, typename SimulationBuffers<ComBufferType>::iterators_ct& it, COM_BUFFER& in_buffer, COM_BUFFER& out_buffer) :
            in_buffer(in_buffer), out_buffer(out_buffer), SimulationBuffers<ComBufferType>(com,it) {}
            protected:
            virtual unsigned char read_byte() {
                const auto bufferLength = std::distance(SimulationBuffers<COM_BUFFER>::comcable.getOutBufferStartRef(),
                SimulationBuffers<COM_BUFFER>::comcable.getOutBufferEndRef());
                if (SimulationBuffers<COM_BUFFER>::itercable.getWriteOffsetRef()>bufferLength)
                    throw SFA::util::runtime_error("Attempted read after end of buffer.",__FILE__,__func__);
                auto next = SimulationBuffers<COM_BUFFER>::itercable.getWriteOffsetRef();
                auto byte = *(SimulationBuffers<COM_BUFFER>::comcable.getOutBufferStartRef()+next);
                next++;
                //auto byte = out_buffer[SimulationBuffers<ComBufferType>::writePos++];
                //if (SimulationBuffers<ComBufferType>::writePos==out_buffer.size())
                if (next>=bufferLength)
                    //SimulationBuffers<ComBufferType>::writePos=0;
                    SimulationBuffers<ComBufferType>::itercable.getWriteOffsetRef() = 0;
                else
                    SimulationBuffers<ComBufferType>::itercable.getWriteOffsetRef() = next;
                return byte;
            }
            virtual void write_byte(unsigned char byte) {
                const auto bufferLength = std::distance(SimulationBuffers<COM_BUFFER>::comcable.getInBufferStartRef(),SimulationBuffers<COM_BUFFER>::comcable.getInBufferStartRef());
                if (SimulationBuffers<COM_BUFFER>::itercable.getReadOffsetRef()>bufferLength)
                    throw SFA::util::runtime_error("Attempted write after end of buffer.",__FILE__,__func__);
                auto next = SimulationBuffers<COM_BUFFER>::itercable.getReadOffsetRef();
                *(SimulationBuffers<COM_BUFFER>::comcable.getInBufferStartRef()+next) = byte;
                next++;
                //in_buffer[SimulationBuffers<ComBufferType>::readPos++]=byte;
                //if (SimulationBuffers<ComBufferType>::readPos==in_buffer.size())
                if (next>=bufferLength)
                    //SimulationBuffers<ComBufferType>::readPos=0;
                    SimulationBuffers<ComBufferType>::itercable.getReadOffsetRef() = 0;
                else
                    SimulationBuffers<ComBufferType>::itercable.getReadOffsetRef() = next;
            }
            private:
            COM_BUFFER& in_buffer;
            const COM_BUFFER& out_buffer;
        };
        template<typename ComBufferType> class MCUSimulationBuffers : SimulationBuffers<ComBufferType> {
            public:
            MCUSimulationBuffers(typename SimulationBuffers<ComBufferType>::buffers_ct& com, typename SimulationBuffers<ComBufferType>::iterators_ct& it, COM_BUFFER& in_buffer, COM_BUFFER& out_buffer) :
            in_buffer(in_buffer), out_buffer(out_buffer), SimulationBuffers<COM_BUFFER>(com,it) {}
            protected:
            virtual unsigned char read_byte() {
                const auto bufferLength = std::distance(SimulationBuffers<COM_BUFFER>::comcable.getInBufferStartRef(),SimulationBuffers<COM_BUFFER>::comcable.getInBufferStartRef());
                if (SimulationBuffers<COM_BUFFER>::itercable.getReadOffsetRef()>bufferLength)
                    throw SFA::util::runtime_error("Attempted read after end of buffer.",__FILE__,__func__);
                auto next = SimulationBuffers<COM_BUFFER>::itercable.getReadOffsetRef();
                auto byte = *(SimulationBuffers<COM_BUFFER>::comcable.getInBufferStartRef()+next);
                next++;
                //auto byte = in_buffer[SimulationBuffers<ComBufferType>::readPos++];
                //if (SimulationBuffers<ComBufferType>::readPos==in_buffer.size())
                if (next>=bufferLength)
                    //SimulationBuffers<ComBufferType>::readPos=0;
                    SimulationBuffers<ComBufferType>::itercable.getReadOffsetRef() = 0;
                else
                    SimulationBuffers<ComBufferType>::itercable.getReadOffsetRef() = next;
                return byte;
            }
            virtual void write_byte(unsigned char byte) {
                const auto bufferLength = std::distance(SimulationBuffers<COM_BUFFER>::comcable.getOutBufferStartRef(),SimulationBuffers<COM_BUFFER>::comcable.getOutBufferEndRef());
                if (SimulationBuffers<COM_BUFFER>::itercable.getWriteOffsetRef()>bufferLength)
                    throw SFA::util::runtime_error("Attempted write after end of buffer.",__FILE__,__func__);
                auto next = SimulationBuffers<COM_BUFFER>::itercable.getWriteOffsetRef();
                *(SimulationBuffers<COM_BUFFER>::comcable.getOutBufferStartRef()+next) = byte;
                next++;
                //out_buffer[SimulationBuffers<ComBufferType>::writePos++]=byte;
                //if (SimulationBuffers<ComBufferType>::writePos==out_buffer.size())
                if (next>=bufferLength)
                    //SimulationBuffers<ComBufferType>::writePos=0;
                    SimulationBuffers<ComBufferType>::itercable.getWriteOffsetRef() = 0;
                else
                    SimulationBuffers<ComBufferType>::itercable.getWriteOffsetRef() = next;
            }
            private:
            const COM_BUFFER& in_buffer;
            COM_BUFFER& out_buffer;
        };
    }
    namespace Behavior{
        template<typename ControllerType, typename... Objects> class SimulationFPGA :
        public SOS::Protocol::SerialFPGA<Objects...>,
        private SOS::Protocol::FPGASimulationBuffers<COM_BUFFER>,
        public SOS::Behavior::EventController<ControllerType> {
            public:
            using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
            SimulationFPGA(bus_type& myBus, COM_BUFFER& in_buffer, COM_BUFFER& out_buffer) :
            SOS::Protocol::SerialFPGA<Objects...>(),
            SOS::Protocol::Serial<Objects...>(),
            SOS::Protocol::FPGASimulationBuffers<COM_BUFFER>(std::get<0>(myBus.const_cables),std::get<0>(myBus.cables),in_buffer,out_buffer),
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
                return SOS::Protocol::FPGASimulationBuffers<COM_BUFFER>::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::FPGASimulationBuffers<COM_BUFFER>::write_byte(byte);
            }
        };
        template<typename ControllerType, typename... Objects> class SimulationMCU :
        public SOS::Protocol::SerialMCU<Objects...>,
        private SOS::Protocol::MCUSimulationBuffers<COM_BUFFER>,
        public SOS::Behavior::EventController<ControllerType> {
            public:
            using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
            SimulationMCU(bus_type& myBus, COM_BUFFER& in_buffer, COM_BUFFER& out_buffer) :
            SOS::Protocol::SerialMCU<Objects...>(),
            SOS::Protocol::Serial<Objects...>(),
            SOS::Protocol::MCUSimulationBuffers<COM_BUFFER>(std::get<0>(myBus.const_cables),std::get<0>(myBus.cables),in_buffer,out_buffer),
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
                return SOS::Protocol::MCUSimulationBuffers<COM_BUFFER>::read_byte();
            }
            virtual void write_byte(unsigned char byte) final {
                SOS::Protocol::MCUSimulationBuffers<COM_BUFFER>::write_byte(byte);
            }
        };
    }
}