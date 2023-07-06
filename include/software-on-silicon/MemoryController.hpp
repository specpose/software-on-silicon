#include "software-on-silicon/RingToMemory.hpp"
#include "software-on-silicon/error.hpp"

namespace SOS{
    namespace MemoryView{
        template<typename ArithmeticType> struct ReadLength : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            ReadLength(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
            auto& getReadBufferStartRef(){return std::get<0>(*this);}
            auto& getReadBufferAfterLastRef(){return std::get<1>(*this);}
        };
        template<typename ArithmeticType> struct ReadOffset : public SOS::MemoryView::TaskCable<ArithmeticType,1> {
            using SOS::MemoryView::TaskCable<ArithmeticType,1>::TaskCable;
            auto& getReadOffsetRef(){return std::get<0>(*this);}
        };
        struct ReaderBus {
            using signal_type = SOS::MemoryView::bus_traits<SOS::MemoryView::BusShaker>::signal_type;
            using _pointer_type = std::array<char,0>::iterator;
            using _difference_type = std::array<char,0>::difference_type;
            using cables_type = std::tuple< ReadOffset<_difference_type> >;
            using const_cables_type = std::tuple< ReadLength<_pointer_type> >;
            ReaderBus(const _pointer_type begin, const _pointer_type end)
            : const_cables(
                std::tuple< ReadLength<_pointer_type> >{ReadLength<_pointer_type>(begin,end)}
                )
            {
                if (std::distance(begin,end)<0)
                    throw SFA::util::runtime_error("Invalid Read Destination",__FILE__,__func__);
                setOffset(0);
            }
            void setOffset(_difference_type offset){
                std::get<0>(cables).getReadOffsetRef().store(offset);
            }
            signal_type signal;
            cables_type cables;
            const_cables_type const_cables;
        };
    }
    namespace Behavior{
        class Reader : public SOS::Behavior::EventLoop<SOS::Behavior::SubController> {
            public:
            using bus_type = SOS::MemoryView::ReaderBus;
            Reader(bus_type::signal_type& outsideSignal) :
            SOS::Behavior::EventLoop<SOS::Behavior::SubController>(outsideSignal){};
            void event_loop(){};
        };
        template<typename S> class WritePriority : public Loop {
        public:
        using bus_type = typename RunLoop<S>::bus_type;
        using subcontroller_type = S;
        WritePriority(
            typename SOS::Behavior::RunLoop<S>::subcontroller_type::bus_type& passThru
            ) : _child(S(passThru,_blocker)) {};
        virtual ~WritePriority(){};
        void event_loop(){}
        protected:
        SOS::MemoryView::BlockerBus _blocker = SOS::MemoryView::BlockerBus{};
        private:
        S _child;
        };
        class ReadTask {
            public:
            using reader_length_ct = std::tuple_element<0,SOS::MemoryView::ReaderBus::const_cables_type>::type;
            using reader_offset_ct = std::tuple_element<0,SOS::MemoryView::ReaderBus::cables_type>::type;
            using blocker_ct = std::tuple_element<0,SOS::MemoryView::BlockerBus::cables_type>::type;
            using blocker_buffer_size = std::tuple_element<1,SOS::MemoryView::BlockerBus::cables_type>::type;
            ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,SOS::MemoryView::BlockerBus& blockerbus) : _length(Length),_offset(Offset), _blocked(blockerbus) {}
            protected:
            void read(){
                auto current = _length.getReadBufferStartRef();
                const auto end = _length.getReadBufferAfterLastRef();
                const auto readOffset = _offset.getReadOffsetRef().load();
                if (std::distance(std::get<1>(_blocked.cables).getBKStartRef().load(),std::get<1>(_blocked.cables).getBKEndRef().load())
                <(std::distance(current,end)+readOffset))
                    throw SFA::util::runtime_error("Read index out of bounds",__FILE__,__func__);
                std::get<0>(_blocked.cables).getBKReaderPosRef().store(
                        std::get<1>(_blocked.cables).getBKStartRef().load()
                        +readOffset
                        );
                while (current!=end){
                    if (!_blocked.signal.getNotifyRef().test_and_set()) {
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
            reader_length_ct& _length;
            reader_offset_ct& _offset;
            SOS::MemoryView::BlockerBus& _blocked;
        };
    }
}