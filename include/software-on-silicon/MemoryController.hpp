namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct ReadSize : private SOS::MemoryView::ConstCable<ArithmeticType,2> {
            ReadSize(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
            auto& getReadBufferStartRef(){return std::get<0>(*this);}
            auto& getReadBufferAfterLastRef(){return std::get<1>(*this);}
        };
        template<typename ArithmeticType> struct ReadOffset : private SOS::MemoryView::TaskCable<ArithmeticType,1> {
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
        template<typename ArithmeticType> struct MemoryControllerBufferSize : private SOS::MemoryView::ConstCable<ArithmeticType,2> {
            MemoryControllerBufferSize(const ArithmeticType& start, const ArithmeticType& end): SOS::MemoryView::ConstCable<ArithmeticType,2>{start,end} {}
            auto& getBKStartRef(){return std::get<0>(*this);}
            auto& getBKEndRef(){return std::get<1>(*this);}
        };
        template<typename MemoryControllerType> struct BlockerBus : public SOS::MemoryView::BusNotifier {
            using _arithmetic_type = typename MemoryControllerType::iterator;
            using const_cables_type = std::tuple< MemoryControllerBufferSize<_arithmetic_type> >;
            BlockerBus(const _arithmetic_type start, const _arithmetic_type end) :
            //tuple requires copy constructor for any tuple that isn't default constructed
            const_cables(
            std::tuple< MemoryControllerBufferSize<_arithmetic_type> >{MemoryControllerBufferSize<_arithmetic_type>(start,end)}
            ){}
            cables_type cables;
            const_cables_type const_cables;
        };
    }
    namespace Behavior {
        template<typename S, typename... Others> class PassthruSimpleController : protected Controller<SOS::MemoryView::Notify,S> {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            using subcontroller_type = typename Controller<SOS::MemoryView::Notify,S>::subcontroller_type;
            PassthruSimpleController(typename bus_type::signal_type& signal, typename subcontroller_type::bus_type& passThru, Others&... args) :
            Controller<SOS::MemoryView::Notify,S>(signal),
            _foreign(passThru),
            _child(subcontroller_type{_foreign, args...})
            {}
            protected:
            typename subcontroller_type::bus_type& _foreign;
            S _child;
        };
        template<typename S, typename... Others> class PassthruEventController : private Controller<SOS::MemoryView::HandShake,S> {
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
            S _child;
        };
        template<typename ReadBufferType, typename MemoryControllerType> class ReadTask {
            public:
            using reader_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<ReadBufferType>::const_cables_type>::type;
            using reader_offset_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<ReadBufferType>::cables_type>::type;
            using memorycontroller_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::BlockerBus<MemoryControllerType>::const_cables_type>::type;
            //only use cables in Tasks
            ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) : _size(Length),_offset(Offset), _memorycontroller_size(blockercable) {}
            protected:
            virtual void read()=0;
            virtual bool wait()=0;
            virtual bool exit_loop()=0;
            reader_length_ct& _size;
            reader_offset_ct& _offset;
            memorycontroller_length_ct& _memorycontroller_size;
        };
        template<typename ReadBufferType, typename MemoryControllerType> class Reader : private SOS::Behavior::EventController<SOS::Behavior::DummyController>,
        public SOS::Behavior::Loop, public virtual SOS::Behavior::ReadTask<ReadBufferType, MemoryControllerType> {
            public:
            using bus_type = typename SOS::MemoryView::BlockerBus<MemoryControllerType>;
            Reader(bus_type& blockerbus, SOS::MemoryView::ReaderBus<ReadBufferType>& outside) :
            _blocked_signal(blockerbus.signal),
            SOS::Behavior::EventController<SOS::Behavior::DummyController>(outside.signal),
            SOS::Behavior::Loop(),
            SOS::Behavior::ReadTask<ReadBufferType, MemoryControllerType>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables)) {}
            ~Reader(){}
            void event_loop() final {
                while(Loop::stop_token.getUpdatedRef().test_and_set()){
                    if (!_intrinsic.getUpdatedRef().test_and_set()){//random access call, FIFO
        //                        std::cout << "S";
                    read();//FIFO whole buffer with intermittent waits when write
        //                        std::cout << "F";
                    _intrinsic.getAcknowledgeRef().clear();
                    }
                }
                Loop::stop_token.getAcknowledgeRef().clear();
            }
            private:
            virtual void read()=0;
            virtual bool wait() {
                if (!_blocked_signal.getNotifyRef().test_and_set()) {//intermittent wait when write
                    _blocked_signal.getNotifyRef().clear();
                    return true;
                } else {
                    return false;
                }
            }
            virtual bool exit_loop() {
                if (!Loop::stop_token.getUpdatedRef().test_and_set()) {
                    Loop::stop_token.getUpdatedRef().clear();
                    return true;
                } else {
                    return false;
                }
            }
            typename bus_type::signal_type& _blocked_signal;
        };
        template<typename BufferType> class MemoryControllerWrite {
            public:
            MemoryControllerWrite() {}
            virtual ~MemoryControllerWrite(){};
            protected:
            virtual void write(const typename BufferType::value_type WORD)=0;
            BufferType memorycontroller = BufferType{};
        };
        template<typename BufferType> class WriteTask : public SOS::Behavior::MemoryControllerWrite<BufferType> {
            public:
            using bus_type = SOS::MemoryView::BlockerBus<BufferType>;//not a controller: bus_type is for superclass
            using SOS::Behavior::MemoryControllerWrite<BufferType>::MemoryControllerWrite;
            protected:
            virtual void write(const typename BufferType::value_type character) {
                if (writerPos!=std::get<0>(_blocker.const_cables).getBKEndRef()) {
                    *(writerPos++)=character;
                } else {
                    throw SFA::util::logic_error("Writer Buffer full",__FILE__,__func__);
                }
            }
            bus_type _blocker = bus_type(this->memorycontroller.begin(),this->memorycontroller.end());
            typename BufferType::iterator writerPos = std::get<0>(_blocker.const_cables).getBKStartRef();
        };
    }
}