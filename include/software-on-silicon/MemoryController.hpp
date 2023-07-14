//#include <iostream>

namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct ReadSize : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            ReadSize(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
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
            using cables_type = std::tuple< ReadOffset<_difference_type> >;
            using const_cables_type = std::tuple< ReadSize<_pointer_type> >;
            ReaderBus(const _pointer_type begin, const _pointer_type end)
            : const_cables(
                std::tuple< ReadSize<_pointer_type> >{ReadSize<_pointer_type>(begin,end)}
                )
            {
                if (std::distance(begin,end)<0)
                    throw SFA::util::runtime_error("Invalid Read Destination",__FILE__,__func__);
                setOffset(0);
            }
            //FIFO requires BusShaker
            void setOffset(_difference_type offset){
                std::get<0>(cables).getReadOffsetRef().store(offset);
            }
            cables_type cables;
            const_cables_type const_cables;
        };
        template<typename ArithmeticType> struct MemoryControllerBufferSize : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            MemoryControllerBufferSize(const ArithmeticType& start, const ArithmeticType& end): SOS::MemoryView::ConstCable<ArithmeticType,2>{start,end} {}
            auto& getBKStartRef(){return std::get<0>(*this);}
            auto& getBKEndRef(){return std::get<1>(*this);}
        };
        template<typename MemoryControllerType> struct BlockerBus{
            using signal_type = SOS::MemoryView::Notify;
            using _arithmetic_type = typename MemoryControllerType::iterator;
            using cables_type = std::tuple< >;
            using const_cables_type = std::tuple< MemoryControllerBufferSize<_arithmetic_type> >;
            BlockerBus(const _arithmetic_type start, const _arithmetic_type end) :
            //tuple requires copy constructor for any tuple that isn't default constructed
            const_cables(
            std::tuple< MemoryControllerBufferSize<_arithmetic_type> >{MemoryControllerBufferSize<_arithmetic_type>(start,end)}
            ){}
            signal_type signal;
            cables_type cables;
            const_cables_type const_cables;
        };
    }
    namespace Behavior {
        template<typename ReadBufferType, typename MemoryControllerType> class ReadTask {
            public:
            using reader_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<ReadBufferType>::const_cables_type>::type;
            using reader_offset_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<ReadBufferType>::cables_type>::type;
            using memorycontroller_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::BlockerBus<MemoryControllerType>::const_cables_type>::type;
            //only use cables in Tasks
            ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) : _size(Length),_offset(Offset), _memorycontroller_size(blockercable) {}
            protected:
            void read(){
                auto current = _size.getReadBufferStartRef();
                const auto end = _size.getReadBufferAfterLastRef();
                const auto readOffset = _offset.getReadOffsetRef().load();
                if (std::distance(_memorycontroller_size.getBKStartRef(),_memorycontroller_size.getBKEndRef())
                <(std::distance(current,end)+readOffset))
                    throw SFA::util::runtime_error("Read index out of bounds",__FILE__,__func__);
                auto readerPos = _memorycontroller_size.getBKStartRef()+readOffset;
                while (current!=end){
                    if (!wait()) {
                        *current = *(readerPos++);
                        ++current;
                    }
                }
            }
            virtual bool wait()=0;
            private:
            reader_length_ct& _size;
            reader_offset_ct& _offset;
            memorycontroller_length_ct& _memorycontroller_size;
        };
        template<typename ReadBufferType, typename MemoryControllerType> class Reader : public SOS::Behavior::EventLoop<SOS::Behavior::SubController>,
                    private SOS::Behavior::ReadTask<ReadBufferType,MemoryControllerType> {
            public:
            using bus_type = typename SOS::MemoryView::ReaderBus<ReadBufferType>;
            Reader(bus_type& outside, SOS::MemoryView::BlockerBus<MemoryControllerType>& blockerbus) :
            _blocked_signal(blockerbus.signal),
            SOS::Behavior::EventLoop<SOS::Behavior::SubController>(outside.signal),
            SOS::Behavior::ReadTask<ReadBufferType,MemoryControllerType>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables))
            {
                _thread = start(this);
            }
            ~Reader(){
                stop_requested = true;
                _thread.join();
            }
            void event_loop(){
                while(!stop_requested){
                    if (!_intrinsic.getUpdatedRef().test_and_set()){//random access call, FIFO
//                        std::cout << "S";
                        SOS::Behavior::ReadTask<ReadBufferType,MemoryControllerType>::read();//FIFO whole buffer with intermittent waits when write
//                        std::cout << "F";
                        _intrinsic.getAcknowledgeRef().clear();
                    }
                }
            }
            bool wait() final {
                if (!_blocked_signal.getNotifyRef().test_and_set()) {//intermittent wait when write
                    _blocked_signal.getNotifyRef().clear();
                    return true;
                } else {
                    return false;
                }
            }
            private:
            bool stop_requested = false;
            std::thread _thread;
            typename SOS::MemoryView::BlockerBus<MemoryControllerType>::signal_type& _blocked_signal;
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
            using SOS::Behavior::MemoryControllerWrite<BufferType>::MemoryControllerWrite;
            protected:
            virtual void write(const typename BufferType::value_type character) {
                if (writerPos!=std::get<0>(_blocker.const_cables).getBKEndRef()) {
                    *(writerPos++)=character;
                } else {
                    throw SFA::util::logic_error("Writer Buffer full",__FILE__,__func__);
                }
            }
            SOS::MemoryView::BlockerBus<BufferType> _blocker = SOS::MemoryView::BlockerBus<BufferType>(this->memorycontroller.begin(),this->memorycontroller.end());
            typename BufferType::iterator writerPos = std::get<0>(_blocker.const_cables).getBKStartRef();
        };
    }
}