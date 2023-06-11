#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>

using namespace SOS::MemoryView;

struct RingBufferBus : public SOS::MemoryView::BusShaker<SOS::RingBuffer>{
    using task_type = SOS::MemoryView::RingBufferIndices;
    task_type wire;
};

class RingBufferImpl : public SOS::RingBuffer {
    public:
    RingBufferImpl(RingBufferBus& bus) : SOS::RingBuffer(bus), _intrinsic(bus) {
        _thread = start(this);
        std::cout<<"RingBuffer started"<<std::endl;
    }
    ~RingBufferImpl(){
        _thread.join();
    }
    void event_loop(){
        operator()();
    }
    void operator()(){
        std::cout<<"Wire Current is "<<std::get<0>(_intrinsic.wire).load()<<std::endl;
    }
    private:
    RingBufferBus& _intrinsic;

    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

auto thread_current(RingBufferIndices& wire){
    return std::get<1>(wire).load();
}

int main(){
    auto bus = RingBufferBus{HandShake{},RingBufferIndices{0,1}};
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (std::get<0>(bus.wire).is_lock_free() &&
    std::get<1>(bus.wire).is_lock_free()){
        try {
        while (true) {
        std::cout << "Before Update Current is " << std::get<0>(bus.wire).load() << std::endl;
        std::cout << "Before Update ThreadCurrent is " << std::get<1>(bus.wire).load() << std::endl;
        auto current = std::get<0>(bus.wire).load();
        if (current!=thread_current(bus.wire)-1){
            std::get<0>(bus.wire).store(current++);
        } else {
            std::get<0>(bus.wire).store(current++);
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        if (get<0>(bus.signal).test_and_set())
            std::cout<<".";
        std::cout << "After Update Current is " << std::get<0>(bus.wire).load() << std::endl;
        std::cout << "After Update ThreadCurrent is " << std::get<1>(bus.wire).load() << std::endl;
        }
        } catch (std::exception& e) {
            delete buffer;
            std::cout << "RingBuffer shutdown"<<std::endl;
        }
    }
}