#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>

using namespace SOS::MemoryView;

template<typename T, size_t WordSize> struct RingBufferBus : public SOS::MemoryView::BusNotifier<SOS::RingBuffer>{
    using task_type = SOS::MemoryView::RingBufferIndices;
    using data_type = SOS::MemoryView::RingBufferData<T,WordSize>;
    task_type wire;
    data_type cache;
};

struct RingBufferBusImpl : public RingBufferBus<bool,1000> {
    //RingBufferBusImpl(size_t n) : RingBufferBus<bool,1000>{} {
    //}
};

class RingBufferImpl : public SOS::RingBuffer {
    public:
    RingBufferImpl(RingBufferBusImpl& bus) : SOS::RingBuffer(bus), _intrinsic(bus) {
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
        std::cout<<"Wire Current is "<<get<RingBufferBusImpl::task_type::FieldName::Current>(_intrinsic.wire).load()<<std::endl;
    }
    private:
    RingBufferBusImpl& _intrinsic;

    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

int main(){
    const size_t n = 1;
    auto bus = RingBufferBusImpl{Notify{},RingBufferIndices{0,1},RingBufferBus<bool,1000>::data_type(n)};
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (get<RingBufferBusImpl::task_type::FieldName::Current>(bus.wire).is_lock_free() &&
    get<RingBufferBusImpl::task_type::FieldName::ThreadCurrent>(bus.wire).is_lock_free()){
        try {
        while (true) {
        std::cout << "Before Update Current is " << get<RingBufferBusImpl::task_type::FieldName::Current>(bus.wire).load() << std::endl;
        std::cout << "Before Update ThreadCurrent is " << get<RingBufferBusImpl::task_type::FieldName::ThreadCurrent>(bus.wire).load() << std::endl;
        auto current = get<RingBufferBusImpl::task_type::FieldName::Current>(bus.wire).load();
        if (current!=get<RingBufferBusImpl::task_type::FieldName::ThreadCurrent>(bus.wire).load()){
            get<RingBufferBusImpl::task_type::FieldName::Current>(bus.wire).store(++current);
            std::cout<<"+";
        } else {
            get<RingBufferBusImpl::task_type::FieldName::Current>(bus.wire).store(++current);
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        if (get<RingBufferBusImpl::signal_type::Status::notify>(bus.signal).test_and_set())
            std::cout<<"=";
        std::cout << "After Update Current is " << get<RingBufferBusImpl::task_type::FieldName::Current>(bus.wire).load() << std::endl;
        std::cout << "After Update ThreadCurrent is " << get<RingBufferBusImpl::task_type::FieldName::ThreadCurrent>(bus.wire).load() << std::endl;
        }
        } catch (std::exception& e) {
            delete buffer;
            std::cout << "RingBuffer shutdown"<<std::endl;
        }
    }
}