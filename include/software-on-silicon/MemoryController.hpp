//#include <iostream>

namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct ReadSize : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType,2>::TaskCable;
            auto& getReadBufferStartRef(){return std::get<0>(*this);}
            auto& getReadBufferAfterLastRef(){return std::get<1>(*this);}
        };
        template<typename ArithmeticType> struct ReadOffset : public SOS::MemoryView::TaskCable<ArithmeticType,1> {
            using SOS::MemoryView::TaskCable<ArithmeticType,1>::TaskCable;
            auto& getReadOffsetRef(){return std::get<0>(*this);}
        };
        template<typename OutputBuffer> struct ReaderBus : public SOS::MemoryView::BusShaker {
            using _pointer_type = typename OutputBuffer::iterator;
            using _difference_type = typename OutputBuffer::difference_type;
            using cables_type = std::tuple< ReadOffset<_difference_type>,ReadSize<_pointer_type> >;
            using const_cables_type = std::tuple< >;
            ReaderBus()
            {
                setOffset(0);
            }
            //FIFO requires BusShaker
            void setOffset(_difference_type offset){
                std::get<0>(cables).getReadOffsetRef().store(offset);
            }
            void setReadBuffer(OutputBuffer& buffer){
                std::get<1>(cables).getReadBufferStartRef().store(buffer.begin());
                std::get<1>(cables).getReadBufferAfterLastRef().store(buffer.end());
            }
            cables_type cables;
            const_cables_type const_cables;
        };
        template<typename ArithmeticType> struct MemoryControllerBufferSize : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            auto& getBKStartRef(){return std::get<0>(*this);}
            auto& getBKEndRef(){return std::get<1>(*this);}
        };
        template<typename MemoryControllerType> struct BlockerBus{
            using signal_type = SOS::MemoryView::Notify;
            using _arithmetic_type = typename MemoryControllerType::iterator;
            using cables_type = std::tuple< MemoryControllerBufferSize<_arithmetic_type> >;
            using const_cables_type = std::tuple< >;
            BlockerBus(const _arithmetic_type start, const _arithmetic_type end) {
                std::get<0>(cables).getBKStartRef().store(start);
                std::get<0>(cables).getBKEndRef().store(start);
            }
            signal_type signal;
            cables_type cables;
            const_cables_type const_cables;
        };
    }
    namespace Behavior {
        template<typename S, typename... Others> class PassthruSimpleController : public Controller<SOS::MemoryView::Notify,S> {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            using subcontroller_type = typename Controller<SOS::MemoryView::Notify,S>::subcontroller_type;//mix-in: could also be local type
            PassthruSimpleController(typename bus_type::signal_type& signal, typename subcontroller_type::bus_type& passThru, Others&... args) :
            Controller<SOS::MemoryView::Notify,S>(signal),
            _foreign(passThru),
            _child(subcontroller_type{_foreign, args...})//_foreign contains BlockerBus, args.. contains ReaderBus
            {}
            protected:
            typename subcontroller_type::bus_type& _foreign;
            private:
            S _child;
        };
        template<typename S, typename... Others> class PassthruEventController : public Controller<SOS::MemoryView::HandShake,S> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            using subcontroller_type = typename Controller<SOS::MemoryView::HandShake,S>::subcontroller_type;
            PassthruEventController(typename bus_type::signal_type& signal, typename subcontroller_type::bus_type& passThru, Others&... args) :
            Controller<SOS::MemoryView::HandShake,S>(signal),
            _foreign(passThru),
            _child(subcontroller_type{_foreign, args...})
            {}
            protected:
            typename subcontroller_type::bus_type& _foreign;
            private:
            S _child;
        };
        template<typename ReadBufferType, typename MemoryControllerType> class ReadTask {
            public:
            using reader_length_ct = typename std::tuple_element<1,typename SOS::MemoryView::ReaderBus<ReadBufferType>::cables_type>::type;
            using reader_offset_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<ReadBufferType>::cables_type>::type;
            using memorycontroller_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::BlockerBus<MemoryControllerType>::cables_type>::type;
            //not variadic, needs _blocked.signal.getNotifyRef()
            ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) : _size(Length),_offset(Offset), _memorycontroller_size(blockercable) {}
            protected:
            void read(){
                auto current = _size.getReadBufferStartRef().load();
                const auto end = _size.getReadBufferAfterLastRef().load();
                const auto readOffset = _offset.getReadOffsetRef().load();
                if (readOffset<0)
                    throw SFA::util::runtime_error("Negative read offset supplied",__FILE__,__func__);
                if (std::distance(_memorycontroller_size.getBKStartRef().load(),_memorycontroller_size.getBKEndRef().load())
                <(std::distance(current,end)+readOffset))
                    throw SFA::util::runtime_error("Read index out of bounds",__FILE__,__func__);
                auto readerPos = _memorycontroller_size.getBKStartRef().load()+readOffset;
                while (current!=end){
                    if (!wait()) {
                        *current = *(readerPos++);
                        ++current;
                    }
                }
            }
            virtual bool wait()=0;
            reader_length_ct& _size;
            reader_offset_ct& _offset;
            memorycontroller_length_ct& _memorycontroller_size;
        };
        template<typename ReadBufferType, typename MemoryControllerType> class Reader : public SOS::Behavior::EventController<SOS::Behavior::DummyController> {
            public:
            using bus_type = typename SOS::MemoryView::BlockerBus<MemoryControllerType>;
            Reader(bus_type& blockerbus, SOS::MemoryView::ReaderBus<ReadBufferType>& outside) :
            _blocked_signal(blockerbus.signal),
            SOS::Behavior::EventController<SOS::Behavior::DummyController>(outside.signal)
            {}
            ~Reader(){}
            virtual void event_loop(){};
            protected:
            bool stop_requested = false;
            typename bus_type::signal_type& _blocked_signal;
        };
        template<typename BufferType> class MemoryControllerWrite {
            public:
            MemoryControllerWrite() {}
            virtual ~MemoryControllerWrite(){};
            protected:
            virtual void write(const typename BufferType::value_type WORD)=0;
            protected:
            BufferType memorycontroller = BufferType{};
        };
        template<typename BufferType> class WriteTask : public SOS::Behavior::MemoryControllerWrite<BufferType> {
            public:
            using bus_type = SOS::MemoryView::BlockerBus<BufferType>;//not a controller: bus_type is for superclass
            using SOS::Behavior::MemoryControllerWrite<BufferType>::MemoryControllerWrite;
            protected:
            virtual void write(const typename BufferType::value_type character) {
                if (writerPos!=std::get<0>(_blocker.cables).getBKEndRef().load()) {
                    *(writerPos++)=character;
                } else {
                    throw SFA::util::logic_error("Writer Buffer full",__FILE__,__func__);
                }
            }
            bus_type _blocker = bus_type(this->memorycontroller.begin(),this->memorycontroller.end());
            typename BufferType::iterator writerPos = std::get<0>(_blocker.cables).getBKStartRef().load();
        };
    }
}