#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>

using namespace SOS::MemoryView;

class RingBufferImpl : public SOS::RingBuffer {
    public:
    RingBufferImpl(SOS::RingBuffer::bus_type& bus) : SOS::RingBuffer(bus) {
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
        std::cout<<"Wire Current is "<<std::get<RingBufferIndices::Current>(_intrinsic.data).load()<<std::endl;
    }
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

auto thread_current(RingBufferIndices& pos){
    return std::get<RingBufferIndices::ThreadCurrent>(pos).load();
}

int main(){
    auto status = HandShake{};
    auto pos = RingBufferIndices{std::atomic{0},std::atomic{1}};
    auto bus = make_bus(pos,status);
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (std::get<RingBufferIndices::Current>(pos).is_lock_free() &&
    std::get<RingBufferIndices::ThreadCurrent>(pos).is_lock_free()){
        try {
        while (true) {
        std::cout << "Before Update Current is " << std::get<RingBufferIndices::Current>(pos).load() << std::endl;
        std::cout << "Before Update ThreadCurrent is " << std::get<RingBufferIndices::ThreadCurrent>(pos).load() << std::endl;
        auto current = std::get<RingBufferIndices::Current>(pos).load();
        if (current!=thread_current(pos)-1){
            std::get<RingBufferIndices::Current>(pos).store(current++);
        } else {
            std::get<RingBufferIndices::Current>(pos).store(current++);
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        if (get<HandShake::Status::updated>(status).test_and_set())
            std::cout<<".";
        std::cout << "After Update Current is " << std::get<RingBufferIndices::Current>(pos).load() << std::endl;
        std::cout << "After Update ThreadCurrent is " << std::get<RingBufferIndices::ThreadCurrent>(pos).load() << std::endl;
        }
        } catch (std::exception& e) {
            delete buffer;
            std::cout << "RingBuffer shutdown"<<std::endl;
        }
    }
}