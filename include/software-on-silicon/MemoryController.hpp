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
    }
}