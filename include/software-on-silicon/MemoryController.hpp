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
                std::get<0>(cables).getReadOffsetRef() = offset;
            }
            void setReadBuffer(OutputBuffer& buffer){
                if (buffer.size()!=std::get<1>(cables).size())
                    throw SFA::util::logic_error("Illegal ReadBuffer size encountered",__FILE__,__func__);
                for (int channel=0;channel<buffer.size();channel++){
                std::get<1>(cables)[channel].getReadBufferStartRef() = buffer[channel].begin();
                std::get<1>(cables)[channel].getReadBufferAfterLastRef() = buffer[channel].end();
                }
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
            using signal_type = SOS::MemoryView::HandShake;
            using _arithmetic_type = typename MemoryControllerType::iterator;
            using cables_type = std::tuple< MemoryControllerBufferSize<_arithmetic_type> >;
            using const_cables_type = std::tuple< >;
            BlockerBus(const _arithmetic_type start, const _arithmetic_type end) {
                std::get<0>(cables).getBKStartRef() = start;
                std::get<0>(cables).getBKEndRef() = start;
            }
            signal_type signal;
            cables_type cables;
            const_cables_type const_cables;
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
        template<typename S, typename... Others> class PassthruSimpleController : public Controller<SOS::MemoryView::Notify,S> {
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
            virtual void read()=0;
            virtual bool wait()=0;
            reader_length_ct& _size;
            reader_offset_ct& _offset;
            memorycontroller_length_ct& _memorycontroller_size;
        };
        template<typename ReadBufferType, typename MemoryControllerType> class Reader : public SOS::Behavior::EventController<SOS::Behavior::DummyController>,
        public SOS::Behavior::Loop {
            public:
            using bus_type = typename SOS::MemoryView::BlockerBus<MemoryControllerType>;
            Reader(bus_type& blockerbus, SOS::MemoryView::ReaderBus<ReadBufferType>& outside) :
            _blocked_signal(blockerbus.signal),
            SOS::Behavior::EventController<SOS::Behavior::DummyController>(outside.signal),
            SOS::Behavior::Loop() {}
            ~Reader(){}
            void event_loop() final {
                while(Loop::stop_token.getUpdatedRef().test_and_set()){
                    fifo_loop();
                }
                Loop::stop_token.getAcknowledgeRef().clear();
            }
            void fifo_loop() {
                if (!_intrinsic.getUpdatedRef().test_and_set()){//random access call, FIFO
        //                        std::cout << "S";
                    read();//FIFO whole buffer with intermittent waits when write
        //                        std::cout << "F";
                    _intrinsic.getAcknowledgeRef().clear();
                }
            }
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
            virtual void acknowledge() {
                _blocked_signal.getAcknowledgeRef().test_and_set();//ended individual read
            }
            protected:
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
        template<typename MemoryControllerType> class WriteTask : public SOS::Behavior::MemoryControllerWrite<MemoryControllerType> {
            public:
            using bus_type = SOS::MemoryView::BlockerBus<MemoryControllerType>;//not a controller: bus_type is for superclass
            using SOS::Behavior::MemoryControllerWrite<MemoryControllerType>::MemoryControllerWrite;
            WriteTask() {}
            protected:
            virtual void write(const typename MemoryControllerType::value_type character) {
                if (writerPos!=std::get<0>(_blocker.cables).getBKEndRef()) {
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
            typename MemoryControllerType::iterator writerPos = std::get<0>(_blocker.cables).getBKStartRef();
            private:
        };
    }
}