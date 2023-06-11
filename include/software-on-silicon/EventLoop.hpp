#pragma once
#include "stackable-functor-allocation/Thread.hpp"

#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        template<typename... T> class TypedWire : public std::tuple<std::atomic<T>...> {
            public:
            using std::tuple<std::atomic<T>...>::tuple;
            TypedWire(T... args) : std::tuple<std::atomic<T>...>{args...} {}
        };
        class Notify : public std::array<std::atomic_flag,1> {
            public:
            enum class Status : int {
                notify
            };
            Notify() : std::array<std::atomic_flag,1>{false} {}
        };
        template<Notify::Status index> auto& get(Notify& signal){
            return std::get<(int)index>(signal);
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
        template<typename T> struct BusNotifier {
            using signal_type = SOS::MemoryView::Notify;
            signal_type signal;
        };
        template<typename T> struct BusShaker {
            using signal_type = SOS::MemoryView::HandShake;
            signal_type signal;
        };
    }
    namespace Behavior {
        class Loop {
            public:
            virtual ~Loop(){};
            virtual void event_loop()=0;
            protected:
            template<typename C> std::thread start(C* startme){
                return std::move(std::thread{std::mem_fn(&C::event_loop),startme});
            }
        };
        class RunLoop : public Loop, public SFA::Lazy {
            public:
            RunLoop() {}
            virtual ~RunLoop() override {};
        };
        class SimpleLoop : public Loop {
            public:
            SimpleLoop(SOS::MemoryView::BusNotifier<SimpleLoop>::signal_type& ground) : _intrinsic(ground) {}
            virtual ~SimpleLoop() override {};
            private:
            typename SOS::MemoryView::BusNotifier<SimpleLoop>::signal_type& _intrinsic;
        };
        class EventLoop : public Loop {
            public:
            EventLoop(SOS::MemoryView::BusShaker<EventLoop>::signal_type& ground) : _intrinsic(ground) {}
            virtual ~EventLoop() override {};
            private:
            typename SOS::MemoryView::BusShaker<EventLoop>::signal_type& _intrinsic;
        };
    }
}