namespace SOS
{
    namespace MemoryView
    {
        template <typename ComIterator>
        struct ComSize : private SOS::MemoryView::ConstCable<ComIterator, 4>
        {
            ComSize(const ComIterator InStart, const ComIterator InEnd, const ComIterator OutStart, const ComIterator OutEnd) : SOS::MemoryView::ConstCable<ComIterator, 4>{InStart, InEnd, OutStart, OutEnd} {}
            auto &getInBufferStartRef() { return std::get<0>(*this); }
            auto &getInBufferEndRef() { return std::get<1>(*this); }
            auto &getOutBufferStartRef() { return std::get<2>(*this); }
            auto &getOutBufferEndRef() { return std::get<3>(*this); }
        };
        template <typename ComIterator>
        struct ComOffset : private SOS::MemoryView::TaskCable<ComIterator, 2>
        {
            ComOffset() : SOS::MemoryView::TaskCable<ComIterator, 2>{0, 0} {}
            auto &getReadOffsetRef() { return std::get<0>(*this); }
            auto &getWriteOffsetRef() { return std::get<1>(*this); }
        };
        template <typename ComBufferType>
        struct ComBus : public bus <
            bus_double_shaker_tag,
            SOS::MemoryView::DoubleHandShake,
            bus_traits<Bus>::cables_type,
            bus_traits<Bus>::const_cables_type
        >
        {
            signal_type signal;
            using const_cables_type = std::tuple<ComSize<typename ComBufferType::iterator>>;
            using cables_type = std::tuple<ComOffset<typename ComBufferType::difference_type>>;
            ComBus(const typename ComBufferType::iterator &inStart, const typename ComBufferType::iterator &inEnd, const typename ComBufferType::iterator &outStart, const typename ComBufferType::iterator &outEnd) : const_cables( {ComSize<typename ComBufferType::iterator>(inStart, inEnd, outStart, outEnd)} )
            {
                if (std::distance(inStart, inEnd) < 1)
                    SFA::util::logic_error(SFA::util::error_code::CombufferSizeIsMinimumWORDSIZE, __FILE__, __func__, typeid(*this).name());
                if (std::distance(outStart, outEnd) < 1)
                    SFA::util::logic_error(SFA::util::error_code::CombufferSizeIsMinimumWORDSIZE, __FILE__, __func__, typeid(*this).name());
                if (std::distance(inStart, inEnd) != std::distance(outStart, outEnd))
                    SFA::util::logic_error(SFA::util::error_code::CombufferInAndOutSizeNotEqual, __FILE__, __func__, typeid(*this).name());
            }
            cables_type cables;
            const_cables_type const_cables;
        };
    }
    namespace Protocol
    {
        template <typename ComBufferType>
        class SimulationBuffers
        {
        public:
            using buffers_ct = typename std::tuple_element<0, typename SOS::MemoryView::ComBus<ComBufferType>::const_cables_type>::type;
            using iterators_ct = typename std::tuple_element<0, typename SOS::MemoryView::ComBus<ComBufferType>::cables_type>::type;
            SimulationBuffers(buffers_ct &com, iterators_ct &it) : comcable(com), itercable(it) {}

        protected:
            virtual unsigned char read_byte()
            {
                const auto bufferLength = std::distance(SimulationBuffers<COM_BUFFER>::comcable.getInBufferStartRef(), SimulationBuffers<COM_BUFFER>::comcable.getInBufferStartRef());
                if (SimulationBuffers<COM_BUFFER>::itercable.getReadOffsetRef().load() > bufferLength)
                    SFA::util::runtime_error(SFA::util::error_code::AttemptedReadAfterEndOfBuffer, __FILE__, __func__, typeid(*this).name());
                auto next = SimulationBuffers<COM_BUFFER>::itercable.getReadOffsetRef().load();
                auto byte = *(SimulationBuffers<COM_BUFFER>::comcable.getInBufferStartRef() + next);
                next++;
                if (next >= bufferLength)
                    SimulationBuffers<ComBufferType>::itercable.getReadOffsetRef().store(0);
                else
                    SimulationBuffers<ComBufferType>::itercable.getReadOffsetRef().store(next);
                return byte;
            }
            virtual void write_byte(unsigned char byte)
            {
                const auto bufferLength = std::distance(SimulationBuffers<COM_BUFFER>::comcable.getOutBufferStartRef(), SimulationBuffers<COM_BUFFER>::comcable.getOutBufferEndRef());
                if (SimulationBuffers<COM_BUFFER>::itercable.getWriteOffsetRef().load() > bufferLength)
                    SFA::util::runtime_error(SFA::util::error_code::AttemptedWriteAfterEndOfBuffer, __FILE__, __func__, typeid(*this).name());
                auto next = SimulationBuffers<COM_BUFFER>::itercable.getWriteOffsetRef().load();
                *(SimulationBuffers<COM_BUFFER>::comcable.getOutBufferStartRef() + next) = byte;
                next++;
                if (next >= bufferLength)
                    SimulationBuffers<ComBufferType>::itercable.getWriteOffsetRef().store(0);
                else
                    SimulationBuffers<ComBufferType>::itercable.getWriteOffsetRef().store(next);
            }
            buffers_ct &comcable;
            iterators_ct &itercable;
        };
    }
    namespace Behavior
    {
        template <typename ControllerType, typename... Objects>
        class SimulationFPGA : public SOS::Protocol::SerialFPGA<Objects...>,
                               private SOS::Protocol::SimulationBuffers<COM_BUFFER>,
                               public SOS::Behavior::BootstrapEventController<ControllerType>
        {
        public:
            using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
            SimulationFPGA(bus_type &myBus) : SOS::Protocol::SerialFPGA<Objects...>(),
                                              SOS::Protocol::Serial<Objects...>(),
                                              SOS::Protocol::SimulationBuffers<COM_BUFFER>(std::get<0>(myBus.const_cables), std::get<0>(myBus.cables)),
                                              SOS::Behavior::BootstrapEventController<ControllerType>(myBus.signal)
            {
                // write_byte(static_cast<unsigned char>(SOS::Protocol::idleState().to_ulong()));//INIT: FPGA initiates communication with an idle byte
                // SOS::Behavior::BootstrapEventController<ControllerType>::_intrinsic.getAcknowledgeRef().clear();//INIT: start one-way handshake
            }
            virtual void event_loop() final { SOS::Protocol::Serial<Objects...>::event_loop(); }

        protected:
            virtual bool descendants_stopped() final { return SOS::Behavior::BootstrapEventController<ControllerType>::descendants_stopped(); }
            virtual constexpr typename SOS::MemoryView::SerialProcessNotifier<Objects...> &foreign() final
            {
                return SOS::Behavior::BootstrapEventController<ControllerType>::_foreign;
            }
            virtual void stop_notifier() final
            {
                SOS::Behavior::BootstrapEventController<ControllerType>::stop_descendants();
            }
        private:
            virtual bool handshake() final
            {
                if (!SOS::Behavior::BootstrapEventController<ControllerType>::_intrinsic.getUpdatedRef().test_and_set())
                {
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final
            {
                SOS::Behavior::BootstrapEventController<ControllerType>::_intrinsic.getAcknowledgeRef().clear(); // SIMULATION: Replace this with getUpdatedRef
            }
            virtual bool aux() final
            {
                if (!SOS::Behavior::BootstrapEventController<ControllerType>::_intrinsic.getAuxUpdatedRef().test_and_set())
                {
                    return true;
                }
                return false;
            }
            virtual void aux_ack() final
            {
                SOS::Behavior::BootstrapEventController<ControllerType>::_intrinsic.getAuxAcknowledgeRef().clear();
            }
            virtual unsigned char read_byte() final
            {
                return SOS::Protocol::SimulationBuffers<COM_BUFFER>::read_byte();
            }
            virtual void write_byte(unsigned char byte) final
            {
                    SOS::Protocol::SimulationBuffers<COM_BUFFER>::write_byte(byte);
            }
        };
        template <typename ControllerType, typename... Objects>
        class SimulationMCU : public SOS::Protocol::SerialMCU<Objects...>,
                              private SOS::Protocol::SimulationBuffers<COM_BUFFER>,
                              public SOS::Behavior::BootstrapEventController<ControllerType>
        {
        public:
            using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
            SimulationMCU(bus_type &myBus) : SOS::Protocol::SerialMCU<Objects...>(),
                                             SOS::Protocol::Serial<Objects...>(),
                                             SOS::Protocol::SimulationBuffers<COM_BUFFER>(std::get<0>(myBus.const_cables), std::get<0>(myBus.cables)),
                                             SOS::Behavior::BootstrapEventController<ControllerType>(myBus.signal)
            {
            }
            virtual void event_loop() final { SOS::Protocol::Serial<Objects...>::event_loop(); }

        protected:
            virtual bool descendants_stopped() final { return SOS::Behavior::BootstrapEventController<ControllerType>::descendants_stopped(); }
            virtual constexpr typename SOS::MemoryView::SerialProcessNotifier<Objects...> &foreign() final
            {
                return SOS::Behavior::BootstrapEventController<ControllerType>::_foreign;
            }
            virtual void stop_notifier() final
            {
                SOS::Behavior::BootstrapEventController<ControllerType>::stop_descendants();
            }
        private:
            virtual bool handshake() final
            {
                if (!SOS::Behavior::BootstrapEventController<ControllerType>::_intrinsic.getUpdatedRef().test_and_set())
                {
                    return true;
                }
                return false;
            }
            virtual void handshake_ack() final
            {
                SOS::Behavior::BootstrapEventController<ControllerType>::_intrinsic.getAcknowledgeRef().clear(); // SIMULATION: Replace this with getUpdatedRef
            }
            virtual bool aux() final
            {
                if (!SOS::Behavior::BootstrapEventController<ControllerType>::_intrinsic.getAuxUpdatedRef().test_and_set())
                {
                    return true;
                }
                return false;
            }
            virtual void aux_ack() final
            {
                SOS::Behavior::BootstrapEventController<ControllerType>::_intrinsic.getAuxAcknowledgeRef().clear();
            }
            virtual unsigned char read_byte() final
            {
                return SOS::Protocol::SimulationBuffers<COM_BUFFER>::read_byte();
            }
            virtual void write_byte(unsigned char byte) final
            {
                    SOS::Protocol::SimulationBuffers<COM_BUFFER>::write_byte(byte);
            }
        };
    }
}
