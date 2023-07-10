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
        template<typename ArithmeticType> struct BlockerCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            auto& getBKReaderPosRef(){return std::get<0>(*this);}
            auto& getBKPosRef(){return std::get<1>(*this);}
        };
        //this is not const_cable because of external dependency
        template<typename ArithmeticType> struct MemoryControllerBufferSize : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            //using SOS::MemoryView::ConstCable<ArithmeticType, 2>::ConstCable;
            MemoryControllerBufferSize(const ArithmeticType& start, const ArithmeticType& end): SOS::MemoryView::ConstCable<ArithmeticType,2>{start,end} {}
            auto& getBKStartRef(){return std::get<0>(*this);}
            auto& getBKEndRef(){return std::get<1>(*this);}
        };
        template<typename MemoryControllerType> struct BlockerBus{
            using signal_type = SOS::MemoryView::Notify;
            using _arithmetic_type = typename MemoryControllerType::iterator;
            using cables_type = std::tuple< BlockerCable<_arithmetic_type> >;
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
            //not variadic, needs _blocked.signal.getNotifyRef()
            ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,typename SOS::MemoryView::BlockerBus<MemoryControllerType>& blockerbus) : _size(Length),_offset(Offset), _blocked(blockerbus) {}
            protected:
            void read(){
                auto current = _size.getReadBufferStartRef();
                const auto end = _size.getReadBufferAfterLastRef();
                const auto readOffset = _offset.getReadOffsetRef().load();
                if (std::distance(std::get<0>(_blocked.const_cables).getBKStartRef(),std::get<0>(_blocked.const_cables).getBKEndRef())
                <(std::distance(current,end)+readOffset))
                    throw SFA::util::runtime_error("Read index out of bounds",__FILE__,__func__);
                std::get<0>(_blocked.cables).getBKReaderPosRef().store(
                        std::get<0>(_blocked.const_cables).getBKStartRef()
                        +readOffset
                        );
                while (current!=end){
                    if (!_blocked.signal.getNotifyRef().test_and_set()) {//intermittent wait when write
                        _blocked.signal.getNotifyRef().clear();
                    } else {
                        auto rP = std::get<0>(_blocked.cables).getBKReaderPosRef().load();
                        *current = *rP;
                        std::get<0>(_blocked.cables).getBKReaderPosRef().store(++rP);
                        ++current;
                    }
                }
            }
            private:
            reader_length_ct& _size;
            reader_offset_ct& _offset;
            typename SOS::MemoryView::BlockerBus<MemoryControllerType>& _blocked;
        };
        /*template<typename BufferType> class Reader : public SOS::Behavior::EventLoop<SOS::Behavior::SubController> {
            public:
            using bus_type = typename SOS::MemoryView::ReaderBus<BufferType>;
            Reader(typename bus_type::signal_type& outsideSignal) :
            SOS::Behavior::EventLoop<SOS::Behavior::SubController>(outsideSignal){};
            void event_loop(){};
        };*/
        template<typename BufferType> class MemoryControllerWrite {
            public:
            MemoryControllerWrite() {}
            virtual ~MemoryControllerWrite(){};
            protected:
            template<typename T> void write(T WORD);
            //only required for external dependency, has to be called at least once in superclass constructor
            virtual void resize(typename BufferType::difference_type newsize){
                throw SFA::util::logic_error("Memory Allocation is not allowed",__FILE__,__func__);
            };
            protected:
            BufferType memorycontroller = BufferType{};
        };
        template<typename BufferType> class WriteTask : public SOS::Behavior::MemoryControllerWrite<BufferType> {
            public:
            WriteTask() :
            SOS::Behavior::MemoryControllerWrite<BufferType>{} {
                std::get<0>(_blocker.cables).getBKPosRef().store(std::get<0>(_blocker.const_cables).getBKStartRef());
                std::get<0>(_blocker.cables).getBKReaderPosRef().store(std::get<0>(_blocker.const_cables).getBKEndRef());
            }
            protected:
            void write(const char character) {
                auto pos = std::get<0>(_blocker.cables).getBKPosRef().load();
                if (pos!=std::get<0>(_blocker.const_cables).getBKEndRef()) {
                    *(pos++)=character;
                    std::get<0>(_blocker.cables).getBKPosRef().store(pos);
                } else {
                    throw SFA::util::logic_error("Writer Buffer full",__FILE__,__func__);
                }
            }
            SOS::MemoryView::BlockerBus<BufferType> _blocker = SOS::MemoryView::BlockerBus<BufferType>(this->memorycontroller.begin(),this->memorycontroller.end());
        };
    }
}