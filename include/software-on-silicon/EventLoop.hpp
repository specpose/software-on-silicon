#pragma once
#include "stackable-functor-allocation/Thread.hpp"

#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        class Notify : public std::array<std::atomic_flag,1> {
            public:
            enum class Status : int {
                notify
            };
            Notify() : std::array<std::atomic_flag,1>{true} {}
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
            HandShake() : std::array<std::atomic_flag,2>{true,true} {}
        };
        template<HandShake::Status index> auto& get(HandShake& signal){
            return std::get<(int)index>(signal);
        };
        template<typename T, size_t N> struct TaskCable : public std::array<std::atomic<T>,N>{
            //using std::array<std::atomic<T>,N>::array;
            using wire_names = enum class empty : unsigned char{} ;
            using cable_arithmetic = T;
        };
        struct Bus {
            using signal_type = std::array<std::atomic_flag,0>;
            using cables_type = std::tuple< >;
            signal_type signal;
            cables_type cables;
        };
        template<typename Bus> struct bus_traits {
            using signal_type = typename Bus::signal_type;
            using cables_type = typename Bus::cables_type;
        };
        struct BusNotifier{
            using signal_type = SOS::MemoryView::Notify;
            using cables_type = bus_traits<Bus>::cables_type;
            signal_type signal;
            cables_type cables;
        };
        struct BusShaker{
            using signal_type = SOS::MemoryView::HandShake;
            using cables_type = bus_traits<Bus>::cables_type;
            signal_type signal;
            cables_type cables;
        };
    }
    namespace Behavior {
        template<typename Task> struct task_traits {
            using cable_type = SOS::MemoryView::TaskCable<void,0>;
        };
        //ArithmeticType derived from SubController::MemoryController::OutputBuffer
        class Task {
            public:
            //arithmetic_type derived from Controller::HostMemory::Bus
            //using cable_arithmetic = typename SOS::MemoryView::TaskCable<T,N>::cable_arithmetic;
            using cable_type = typename task_traits<Task>::cable_type;
            //Task(cable_type& taskitem) {};
            //virtual ~Task() {};
        };
        class Loop {
            public:
            using bus_type = SOS::MemoryView::Bus;
            virtual ~Loop(){};
            virtual void event_loop()=0;
            protected:
            template<typename C> static std::thread start(C* startme){
                return std::move(std::thread{std::mem_fn(&C::event_loop),startme});
            }
        };
        class RunLoop : public Loop, public SFA::Lazy {
            public:
            using bus_type = SOS::MemoryView::Bus;
            RunLoop() {}
            virtual ~RunLoop() override {};
        };
        class SimpleLoop : public Loop {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            SimpleLoop(bus_type::signal_type& ground) {}
            virtual ~SimpleLoop() override {};
        };
        class EventLoop : public Loop {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            EventLoop(bus_type::signal_type& ground) {}
            virtual ~EventLoop() override {};
        };
    }
}