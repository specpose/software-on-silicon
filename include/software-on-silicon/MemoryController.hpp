#include <stddef.h>
#include <array>
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
                    SFA::util::logic_error(SFA::util::error_code::IllegalReadbufferSizeEncountered,__FILE__,__func__);
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
        template<typename MemoryControllerType> struct BlockerBus : public SOS::MemoryView::BusShaker {
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
                    SFA::util::logic_error(SFA::util::error_code::ArachannelInitializationError,__FILE__,__func__);
            }
            T* end(){
                if (p)
                    return p+size;
                else
                    SFA::util::logic_error(SFA::util::error_code::ArachannelInitializationError,__FILE__,__func__);
            }
            private:
            T* p = nullptr;
            const std::size_t size;
        };
        template<typename DG> struct reader_traits : public SFA::DeductionGuide<DG> {
        };
    }
    namespace Behavior {
        template<typename S, typename... Others> class PassthruAsyncController : public Controller<S>, public Loop {
            public:
            PassthruAsyncController(typename S::bus_type& passThru, Others&... args) :
            Controller<S>(), Loop(), _foreign(passThru), _child(S{_foreign, args...}) {}
            protected:
            typename S::bus_type& _foreign;
            private:
            S _child;
        };
        template<typename S, typename... Others> class PassthruSimpleController : public Controller<S>, public Loop, protected SimpleSubController {
            public:
            PassthruSimpleController(typename bus_type::signal_type& signal, typename S::bus_type& passThru, Others&... args) :
            Controller<S>(),
            Loop(),
            SimpleSubController(signal),
            _foreign(passThru),
            _child(S{_foreign, args...})
            {}
            protected:
            typename S::bus_type& _foreign;
            private:
            S _child;
        };
        template<typename S, typename... Others> class PassthruEventController : public Controller<S>, public Loop, protected EventSubController {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            PassthruEventController(typename bus_type::signal_type& signal, typename S::bus_type& passThru, Others&... args) :
            Controller<S>(),
            Loop(),
            EventSubController(signal),
            _foreign(passThru),
            _child(S{_foreign, args...})
            {}
            protected:
            typename S::bus_type& _foreign;
            private:
            S _child;
        };
        template<typename MemoryControllerType> class ReadTask {
            public:
            using reader_length_ct = typename std::tuple_element<1,typename SOS::MemoryView::ReaderBus<typename SOS::MemoryView::reader_traits<MemoryControllerType>::input_container_type>::cables_type>::type;
            using reader_offset_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<typename SOS::MemoryView::reader_traits<MemoryControllerType>::input_container_type>::cables_type>::type;
            using memorycontroller_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::BlockerBus<MemoryControllerType>::cables_type>::type;
            //not variadic, needs _blocked.signal.getNotifyRef()
            ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) : _size(Length),_offset(Offset), _memorycontroller_size(blockercable) {}
            protected:
            virtual void read()=0;
            virtual bool wait()=0;
            virtual void wait_acknowledge()=0;
            //virtual bool exit_loop()=0;
            reader_length_ct& _size;
            reader_offset_ct& _offset;
            memorycontroller_length_ct& _memorycontroller_size;
        };
        template<typename MemoryControllerType> class Reader : public SOS::Behavior::BootstrapDummyEventController<>,
        public virtual SOS::Behavior::ReadTask<MemoryControllerType> {
            public:
            using bus_type = typename SOS::MemoryView::ReaderBus<typename SOS::MemoryView::reader_traits<MemoryControllerType>::input_container_type>;
            Reader(bus_type& outside, SOS::MemoryView::BlockerBus<MemoryControllerType>& blockerbus) :
            _blocked_signal(blockerbus.signal),
            SOS::Behavior::BootstrapDummyEventController<>(outside.signal)
            {}
            ~Reader(){}
            void event_loop() final {
                while(Stoppable::is_running()){
                    if (!_intrinsic.getUpdatedRef().test_and_set()){//random access call, FIFO
        //                        std::cout << "S";
                    read();//FIFO whole buffer with intermittent waits when write
        //                        std::cout << "F";
                    _intrinsic.getAcknowledgeRef().clear();
                    }
                }
                Stoppable::finished();
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
            virtual void wait_acknowledge() {
                _blocked_signal.getAcknowledgeRef().test_and_set();//ended individual read
            }
            typename SOS::MemoryView::BlockerBus<MemoryControllerType>::signal_type& _blocked_signal;
        };
        template<typename MemoryControllerType> class MemoryControllerWrite {
            public:
            MemoryControllerWrite() {}
            virtual ~MemoryControllerWrite(){};
            protected:
            virtual void write(const typename MemoryControllerType::value_type WORD)=0;
            MemoryControllerType memorycontroller = MemoryControllerType{};
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
                        SFA::util::logic_error(SFA::util::MemorycontrollerHasNotBeenInitialized,__FILE__,__func__);
                    if ((**writerPos).size()!=(*character).size())
                        SFA::util::logic_error(SFA::util::IllegalCharactersizeEncountered,__FILE__,__func__);
                    for(std::size_t channel=0;channel<(**writerPos).size();channel++)
                        (**writerPos)[channel]=(*character)[channel];
                    writerPos++;
                } else {
                    SFA::util::logic_error(SFA::util::error_code::WriterBufferFull,__FILE__,__func__);
                }
            }
            bus_type _blocker = bus_type(this->memorycontroller.begin(),this->memorycontroller.end());
            typename MemoryControllerType::iterator writerPos = std::get<0>(_blocker.cables).getBKStartRef().load();
            private:
        };
    }
}