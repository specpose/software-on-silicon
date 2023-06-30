#include "software-on-silicon/EventLoop.hpp"
#include <chrono>
#include "software-on-silicon/error.hpp"

using namespace std::chrono;

namespace SOS{
    namespace MemoryView{
        template<typename ArithmeticType> struct ReadLength : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            enum class wire_names : unsigned char { startPos, afterLast };
        };
        template<typename ArithmeticType, typename ReadLength<ArithmeticType>::wire_names index> auto& get(ReadLength<ArithmeticType>& cable){
            return std::get<(unsigned char)index>(cable);
        };
        template<typename ArithmeticType> struct ReadOffset : public SOS::MemoryView::TaskCable<ArithmeticType,1> {
            using SOS::MemoryView::TaskCable<ArithmeticType,1>::TaskCable;
            enum class wire_names : unsigned char{ readOffset } ;
        };
        template<typename ArithmeticType, typename ReadOffset<ArithmeticType>::wire_names index> auto& get(ReadOffset<ArithmeticType>& cable){
            return std::get<(unsigned char)index>(cable);
        };
        struct ReaderBus {
            using signal_type = SOS::MemoryView::bus_traits<SOS::MemoryView::BusShaker>::signal_type;
            using _pointer_type = std::array<char,0>::iterator;
            using _difference_type = std::array<char,0>::difference_type;
            using cables_type = std::tuple< ReadLength<_pointer_type>, ReadOffset<_difference_type> >;
            ReaderBus(_pointer_type begin, _pointer_type end) :
            start(begin),
            end(end){
                get<_pointer_type,ReadLength<_pointer_type>::wire_names::startPos>(get<0>(cables)).store(begin);
                if (std::distance(start,end)<0)
                    throw SFA::util::runtime_error("Invalid Read Destination",__FILE__,__func__);
                get<_pointer_type,ReadLength<_pointer_type>::wire_names::afterLast>(get<0>(cables)).store(end);
            }
            void setOffset(_difference_type offset){
                get<_difference_type,ReadOffset<_difference_type>::wire_names::readOffset>(std::get<1>(cables)).store(offset);
            }
            signal_type signal;
            cables_type cables;
            _pointer_type start,end;
        };
        template<typename ArithmeticType> struct BlockerCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            enum class wire_names : unsigned char{ readerPos, pos } ;
        };
        template<typename ArithmeticType, typename SOS::MemoryView::BlockerCable<ArithmeticType>::wire_names index> auto& get(
            SOS::MemoryView::BlockerCable<ArithmeticType>& cable
            ){
            return std::get<(unsigned char)index>(cable);
        };
        struct BlockerBus{
            using signal_type = SOS::MemoryView::Notify;
            using _arithmetic_type = ReaderBus::_pointer_type;
            using cables_type = std::tuple< BlockerCable<_arithmetic_type> >;
            signal_type signal;
            cables_type cables;
            _arithmetic_type start,end;
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
        using bus_type = typename SimpleLoop<S>::bus_type;
        using subcontroller_type = S;
        WritePriority(
            typename SOS::Behavior::SimpleLoop<S>::bus_type::signal_type& mySignal,
            typename SOS::Behavior::SimpleLoop<S>::subcontroller_type::bus_type& passThru
            ) : _intrinsic(mySignal), _child(S(passThru,_blocker)) {};
        virtual ~WritePriority(){};
        void event_loop(){}
        protected:
        typename bus_type::signal_type& _intrinsic;
        SOS::MemoryView::BlockerBus _blocker = SOS::MemoryView::BlockerBus{};
        private:
        S _child;
        };
    }
}