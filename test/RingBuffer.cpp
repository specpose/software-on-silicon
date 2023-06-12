#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>

using namespace SOS::MemoryView;

struct RingBufferBus : public SOS::MemoryView::BusNotifier<SOS::RingBuffer>{
    using task_type = SOS::MemoryView::RingBufferIndices;
    task_type wire;
};

class RingBufferImpl : public SOS::RingBuffer {
    public:
    RingBufferImpl(RingBufferBus& bus) : SOS::RingBuffer(bus), _intrinsic(bus) {
        _thread = start(this);
        std::cout<<"RingBuffer started"<<std::endl;
    }
    ~RingBufferImpl() final{
        _thread.join();
    }
    void event_loop(){
        operator()();
    }
    void operator()(){
        std::cout<<"Wire Current is "<<get<RingBufferBus::task_type::FieldName::Current>(_intrinsic.wire).load()<<std::endl;
    }
    private:
    RingBufferBus& _intrinsic;

    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

auto thread_current(RingBufferIndices& wire){
    return get<RingBufferBus::task_type::FieldName::ThreadCurrent>(wire).load();
}

int main(){
    auto bus = RingBufferBus{Notify{},RingBufferIndices{0,1}};
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (get<RingBufferBus::task_type::FieldName::Current>(bus.wire).is_lock_free() &&
    get<RingBufferBus::task_type::FieldName::ThreadCurrent>(bus.wire).is_lock_free()){
        try {
        while (true) {
        std::cout << "Before Update Current is " << get<RingBufferBus::task_type::FieldName::Current>(bus.wire).load() << std::endl;
        std::cout << "Before Update ThreadCurrent is " << get<RingBufferBus::task_type::FieldName::ThreadCurrent>(bus.wire).load() << std::endl;
        auto current = get<RingBufferBus::task_type::FieldName::Current>(bus.wire).load();
        if (current!=thread_current(bus.wire)-1){
            get<RingBufferBus::task_type::FieldName::Current>(bus.wire).store(current++);
        } else {
            get<RingBufferBus::task_type::FieldName::Current>(bus.wire).store(current++);
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        if (get<RingBufferBus::signal_type::Status::notify>(bus.signal).test_and_set())
            std::cout<<"=";
        std::cout << "After Update Current is " << get<RingBufferBus::task_type::FieldName::Current>(bus.wire).load() << std::endl;
        std::cout << "After Update ThreadCurrent is " << get<RingBufferBus::task_type::FieldName::ThreadCurrent>(bus.wire).load() << std::endl;
        }
        } catch (std::exception& e) {
            delete buffer;
            std::cout << "RingBuffer shutdown"<<std::endl;
        }
    }
}