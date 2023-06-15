#include "software-on-silicon/RingBuffer.hpp"
#include <atomic>
#include <array>
#include <iostream>

class Notify2 : public std::array<std::atomic_flag,1> {
    public:
    enum class Status : int {
        notify
    };
    Notify2() : std::array<std::atomic_flag,1>{false} {}
};

template<typename T> struct BusNotifier2 {
    using signal_type = Notify2;
    signal_type signal;
};

class Dummy {};

template<typename ArithmeticType> struct TaskBus2 : public BusNotifier2<Dummy> {
    public:
    using arithmetic_type = ArithmeticType;
    using BusNotifier2<Dummy>::BusNotifier2;
};

template<typename ArithmeticType> struct RingBufferBus2 : public TaskBus2<ArithmeticType> {
    public:
    RingBufferBus2() : TaskBus2<ArithmeticType>{} {
    }
};

using buffer_iterator = std::array<double,1000>::iterator;
/*template<> struct RingBufferBus2<buffer_iterator> : public TaskBus2<buffer_iterator> {
    public:
    RingBufferBus2() : TaskBus2<buffer_iterator>{} {
    }
};*/

using namespace SOS::MemoryView;
template<> struct RingBufferBus2< buffer_iterator > : public TaskBus2<buffer_iterator> {
    public:
    RingBufferBus2() :
    buffer(std::array<double,1000>{})
    ,task1(RingBufferTaskCable<buffer_iterator>{buffer.begin(),buffer.begin(),buffer.end()})
    {}
    RingBufferTaskCable<buffer_iterator> task1;
    private:
    std::array<double,1000> buffer;
};

int main(){
    auto bus = BusNotifier2<Dummy>{};
    std::cout<<"BusNotifier<Dummy> signal type is "<<typeid(bus.signal).name()<<std::endl;
    auto bus2 = BusNotifier2<Dummy>{};
    std::cout<<"BusNotifier<SimpleLoop> signal type is "<<typeid(bus2.signal).name()<<std::endl;
    auto bus3 = TaskBus2<std::array<double,1000>::iterator>{};
    std::cout<<"TaskBus signal type is "<<typeid(bus3.signal).name()<<std::endl;
    auto bus4 = RingBufferBus2<std::array<double,1000>::iterator>{};
    std::cout<<"RingBufferBus signal type is "<<typeid(bus4.signal).name()<<std::endl;
    std::cout<<"RingBufferBus task1 type is "<<typeid(bus4.task1).name()<<std::endl;
}