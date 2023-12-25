namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct ReadSize : private SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType,2>::TaskCable;
            auto& getReadBufferStartRef(){return std::get<0>(*this);}
            auto& getReadBufferAfterLastRef(){return std::get<1>(*this);}
        };
        template<typename ArithmeticType> struct ReadOffset : private SOS::MemoryView::TaskCable<ArithmeticType,1> {
            using SOS::MemoryView::TaskCable<ArithmeticType,1>::TaskCable;
            auto& getReadOffsetRef(){return std::get<0>(*this);}
        };
        template<typename OutputBuffer> struct ReaderBus : public SOS::MemoryView::BusShaker {
            using _pointer_type = typename OutputBuffer::value_type::iterator;
            using _difference_type = typename OutputBuffer::value_type::difference_type;
            using cables_type = std::tuple< ReadOffset<_difference_type>,std::vector<ReadSize<_pointer_type>> >;
            using const_cables_type = std::tuple< >;
            ReaderBus(const std::size_t vst_numInputs)
            {
                std::get<1>(cables) = std::vector<ReadSize<_pointer_type>>(vst_numInputs);
                setOffset(0);
            }
            //FIFO requires BusShaker
            void setOffset(_difference_type offset){
                std::get<0>(cables).getReadOffsetRef().store(offset);
            }
            void setReadBuffer(OutputBuffer& buffer){
                if (buffer.size()!=std::get<1>(cables).size())
                    throw SFA::util::logic_error("Illegal ReadBuffer size encountered",__FILE__,__func__);
                for (int channel=0;channel<buffer.size();channel++){
                std::get<1>(cables)[channel].getReadBufferStartRef().store(buffer[channel].begin());
                std::get<1>(cables)[channel].getReadBufferAfterLastRef().store(buffer[channel].end());
                }
            }
            cables_type cables;
            const_cables_type const_cables;
        };
        template<typename ArithmeticType> struct MemoryControllerBufferSize : private SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            auto& getBKStartRef(){return std::get<0>(*this);}
            auto& getBKEndRef(){return std::get<1>(*this);}
        };
        template<typename MemoryControllerType> struct BlockerBus : public SOS::MemoryView::BusShaker{
            using _arithmetic_type = typename MemoryControllerType::iterator;
            using cables_type = std::tuple< MemoryControllerBufferSize<_arithmetic_type> >;
            BlockerBus(const _arithmetic_type start, const _arithmetic_type end) {
                std::get<0>(cables).getBKStartRef().store(start);
                std::get<0>(cables).getBKEndRef().store(start);
            }
            cables_type cables;
        };
        template<typename T> class ARAChannel {
            public:
            using iterator = T*;
            using difference_type = std::size_t;
            ARAChannel(T* begin,const std::size_t size) : p(begin),size(size) {}
            ~ARAChannel(){
                p=nullptr;
            }
            T* begin(){
                if (p)
                    return p;
                else
                    throw SFA::util::logic_error("ARAChannel initialization error",__FILE__,__func__);
            }
            T* end(){
                if (p)
                    return p+size;
                else
                    throw SFA::util::logic_error("ARAChannel initialization error",__FILE__,__func__);
            }
            private:
            T* p = nullptr;
            const std::size_t size;
        };
    }
    namespace Behavior {
        template<typename S, typename... Others> class PassthruAsyncController : private _Async<S> {
            public:
            PassthruAsyncController(typename _Async<S>::subcontroller_type::bus_type& blocker, Others&... args) :
            _Async<S>(), _foreign(blocker), _child(typename _Async<S>::subcontroller_type{_foreign, args...}) {}
            protected:
            typename _Async<S>::subcontroller_type::bus_type& _foreign;
            typename _Async<S>::subcontroller_type _child;
        };
        template<typename S, typename... Others> class PassthruSimpleController : public SOS::Behavior::_Async<S>, protected _Controller<SOS::MemoryView::Notify> {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            PassthruSimpleController(typename bus_type::signal_type& signal, typename _Async<S>::subcontroller_type::bus_type& passThru, Others&... args) :
            _Async<S>(),
            _Controller<SOS::MemoryView::Notify>(signal),
            _foreign(passThru),
            _child(typename _Async<S>::subcontroller_type{_foreign, args...})
            {}
            protected:
            typename _Async<S>::subcontroller_type::bus_type& _foreign;
            typename _Async<S>::subcontroller_type _child;
        };
        template<typename S, typename... Others> class PassthruEventController : public SOS::Behavior::_Async<S>, private _Controller<SOS::MemoryView::HandShake> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            PassthruEventController(typename bus_type::signal_type& signal, typename SOS::Behavior::_Async<S>::subcontroller_type::bus_type& passThru, Others&... args) :
            _Async<S>(),
            _Controller<SOS::MemoryView::HandShake>(signal),
            _foreign(passThru),
            _child(typename SOS::Behavior::_Async<S>::subcontroller_type{_foreign, args...})
            {}
            protected:
            typename SOS::Behavior::_Async<S>::subcontroller_type::bus_type& _foreign;
            typename SOS::Behavior::_Async<S>::subcontroller_type _child;
        };
        template<typename ReadBufferType, typename MemoryControllerType> class ReadTask {
            public:
            using reader_length_ct = typename std::tuple_element<1,typename SOS::MemoryView::ReaderBus<ReadBufferType>::cables_type>::type;
            using reader_offset_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<ReadBufferType>::cables_type>::type;
            using memorycontroller_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::BlockerBus<MemoryControllerType>::cables_type>::type;
            //not variadic, needs _blocked.signal.getNotifyRef()
            ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) : _size(Length),_offset(Offset), _memorycontroller_size(blockercable) {}
            protected:
            virtual void read()=0;
            virtual bool wait()=0;
            virtual bool exit_loop()=0;
            virtual void acknowledge()=0;
            reader_length_ct& _size;
            reader_offset_ct& _offset;
            memorycontroller_length_ct& _memorycontroller_size;
        };
        template<typename ReadBufferType, typename MemoryControllerType> class Reader : private SOS::Behavior::DummyEventController<>,
        public SOS::Behavior::Loop, public virtual SOS::Behavior::ReadTask<ReadBufferType, MemoryControllerType> {
            public:
            using bus_type = typename SOS::MemoryView::BlockerBus<MemoryControllerType>;
            Reader(bus_type& blockerbus, SOS::MemoryView::ReaderBus<ReadBufferType>& outside) :
            _blocked_signal(blockerbus.signal),
            SOS::Behavior::DummyEventController<>(outside.signal),
            SOS::Behavior::Loop()
            {}
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
                if (!_blocked_signal.getUpdatedRef().test_and_set()) {//intermittent wait when write
                    _blocked_signal.getUpdatedRef().clear();
                    return true;
                } else {
                    _blocked_signal.getAcknowledgeRef().clear();//started individual read
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
            protected:
            virtual void acknowledge() {
                _blocked_signal.getAcknowledgeRef().test_and_set();//ended individual read
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
        template<typename MemoryControllerType> class WriteTask : public SOS::Behavior::MemoryControllerWrite<MemoryControllerType> {
            public:
            using bus_type = SOS::MemoryView::BlockerBus<MemoryControllerType>;//not a controller: bus_type is for superclass
            using SOS::Behavior::MemoryControllerWrite<MemoryControllerType>::MemoryControllerWrite;
            WriteTask() {}
            protected:
            virtual void write(const typename MemoryControllerType::value_type character) {
                if (writerPos!=std::get<0>(_blocker.cables).getBKEndRef().load()) {
                    if (!(*writerPos))
                        throw SFA::util::logic_error("memorycontroller has not been initialized",__FILE__,__func__);
                    if ((**writerPos).size()!=(*character).size())
                        throw SFA::util::logic_error("Illegal character size encountered",__FILE__,__func__);
                    for(std::size_t channel=0;channel<(**writerPos).size();channel++)
                        (**writerPos)[channel]=(*character)[channel];
                    writerPos++;
                } else {
                    throw SFA::util::logic_error("Writer Buffer full",__FILE__,__func__);
                }
            }
            bus_type _blocker = bus_type(this->memorycontroller.begin(),this->memorycontroller.end());
            typename MemoryControllerType::iterator writerPos = std::get<0>(_blocker.cables).getBKStartRef().load();
            private:
        };
    }
}