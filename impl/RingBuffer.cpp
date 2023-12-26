#include "software-on-silicon/error.hpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include <iostream>

#define RING_BUFFER std::array<char,33>

using namespace SOS::MemoryView;

class RingBufferTaskImpl : protected SOS::Behavior::RingBufferTask<RING_BUFFER> {
    public:
    using cable_type = std::tuple_element<0,RingBufferBus<RING_BUFFER>::cables_type>::type;
    using const_cable_type = std::tuple_element<0,RingBufferBus<RING_BUFFER>::const_cables_type>::type;
    RingBufferTaskImpl(cable_type& indices, const_cable_type& bounds) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds){}
    private:
    virtual void write(const RING_BUFFER::value_type character) final {std::cout<<character;}
};
class RingBufferImpl : private SOS::Behavior::DummySimpleController<>, private RingBufferTaskImpl, public SOS::Behavior::Loop {
    public:
    RingBufferImpl(RingBufferBus<RING_BUFFER>& bus) :
    SOS::Behavior::DummySimpleController<>(bus.signal),
    RingBufferTaskImpl(std::get<0>(bus.cables),std::get<0>(bus.const_cables)),
    SOS::Behavior::Loop()
    {
        _thread = start(this);
    }
    ~RingBufferImpl() final{
        //stop_token.getUpdatedRef().clear();
        //_thread.join();
        _thread.detach();
    }
    void event_loop(){
        while(stop_token.getUpdatedRef().test_and_set()){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                RingBufferTask::read_loop();
            }
            std::this_thread::yield();
        }
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};