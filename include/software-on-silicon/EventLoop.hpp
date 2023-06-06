#include "stackable-functor-allocation/Thread.hpp"

#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        template<typename... T> class TypedWire : public std::tuple<T...> {
            public:
            using std::tuple<T...>::tuple;
        };
        //1+1=0
        enum HandShakeStatus{
            updated,
            ack
        };
        class HandShake : public std::array<std::atomic_flag,2> {
            public:
            using Status = SOS::MemoryView::HandShakeStatus;
        };
        template<typename TypedWire, typename SignalWire=SOS::MemoryView::HandShake> struct Bus {
            using data_type = TypedWire;
            SignalWire& signal;
            TypedWire& data;
        };
        template<typename TypedWire> SOS::MemoryView::Bus<TypedWire> make_bus(
            SOS::MemoryView::HandShake& twoLines,
            TypedWire& wire) {
            auto const bus = SOS::MemoryView::Bus<TypedWire>{twoLines, wire};
            return bus;
        };
    }
    namespace Behavior {
        template<typename T> class EventLoop : public SFA::Lazy{
            public:
            using bus_type = T;
            EventLoop(bus_type& combinedbus) : _intrinsic(combinedbus) {}
            virtual ~EventLoop(){}
            virtual void event_loop()=0;
            protected:
            template<typename C> std::thread start(C* startme){
                return std::move(std::thread{std::mem_fn(&C::event_loop),startme});
            }
            bus_type& _intrinsic;
        };
    }
}