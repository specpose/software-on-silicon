#pragma once
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
        class HandShake : public std::array<std::atomic_flag,2> {
            public:
            enum class Status : int {
                updated,
                ack
            };
            HandShake() : std::array<std::atomic_flag,2>{false,false} {}
        };
        template<HandShake::Status index> auto& get(HandShake& signal){
            return std::get<(int)index>(signal);
        };
        template<typename TypedWire, typename SignalWire> struct Bus {
            using data_type = TypedWire;
            using signal_type = SignalWire;
            TypedWire& data;
            SignalWire& signal;
        };
        template<typename TypedWire> SOS::MemoryView::Bus<TypedWire,SOS::MemoryView::HandShake> make_bus(
            TypedWire& wire,
            SOS::MemoryView::HandShake& signal
            ) {
            auto const bus = SOS::MemoryView::Bus<TypedWire,SOS::MemoryView::HandShake>{wire, signal};
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