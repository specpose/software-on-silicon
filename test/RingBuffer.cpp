#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>

using namespace SOS::MemoryView;

class RingBufferImpl : public SOS::RingBuffer {
    public:
    RingBufferImpl(SOS::RingBuffer::SignalType& signalbus,TypedWireImpl& databus) : SOS::RingBuffer(signalbus,databus) {
        _thread = start(this);
        std::cout<<"RingBuffer started"<<std::endl;
    }
    ~RingBufferImpl(){
        _thread.join();
    }
    void event_loop(){
        task<Update::updated>(_foreignData);
    }
    //Indexes should match in signals and wires!
    template<size_t SignalNumber> static void task(TypedWireImpl& wire){
        auto& signal = std::get<SignalNumber>(wire);
        std::cout<<"Wire Current is "<<signal.load()<<std::endl;
    }
    /*template<> static void task<Update>(TypedWireImpl& wire) {
        auto signal = std::get<Update>(wire);
    }*/
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

auto thread_current(TypedWireImpl& pos){
    return std::get<TypedWireImpl::ThreadCurrent>(pos).load();
}

int main(){
    auto status = Update{false};
    auto pos = TypedWireImpl{0,1};
    RingBufferImpl* buffer = new RingBufferImpl(status,pos);
    if (std::get<TypedWireImpl::Current>(pos).is_lock_free() &&
    std::get<TypedWireImpl::ThreadCurrent>(pos).is_lock_free()){
        try {
        while (true) {
        std::cout << "Before Update Current is " << std::get<TypedWireImpl::Current>(pos).load() << std::endl;
        std::cout << "Before Update ThreadCurrent is " << std::get<TypedWireImpl::Current>(pos).load() << std::endl;
        auto current = std::get<TypedWireImpl::Current>(pos).load();
        if (current!=thread_current(pos)-1){
            std::get<TypedWireImpl::Current>(pos).store(current++);
        } else {
            std::get<TypedWireImpl::Current>(pos).store(current++);
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        if (std::get<Update::updated>(status).test_and_set())
            std::cout<<".";
        std::cout << "After Update Current is " << std::get<TypedWireImpl::Current>(pos).load() << std::endl;
        std::cout << "After Update ThreadCurrent is " << std::get<TypedWireImpl::Current>(pos).load() << std::endl;
        }
        } catch (std::exception& e) {
            delete buffer;
            std::cout << "RingBuffer shutdown"<<std::endl;
        }
    }
}