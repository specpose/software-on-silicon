#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/RingBuffer.hpp"

#include "Sample.cpp"
#define SAMPLE_TYPE char
#define MAX_BLINK 1
using RING_BUFFER=std::array<std::array<SOS::MemoryView::sample<SAMPLE_TYPE,1>,MAX_BLINK>,34>;//INTERLEAVED

using namespace SOS::MemoryView;

class RingBufferTaskImpl : protected SOS::Behavior::RingBufferTask<RING_BUFFER> {
    public:
    using cable_type = std::tuple_element<0,RingBufferBus<RING_BUFFER>::cables_type>::type;
    using const_cable_type = std::tuple_element<0,RingBufferBus<RING_BUFFER>::const_cables_type>::type;
    RingBufferTaskImpl(cable_type& indices, const_cable_type& bounds) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds){}
    private:
    virtual void transfer(RING_BUFFER::value_type& character) final {std::cout<<character[0].channels[0];}//HACK: hard coded single sample, hard coded channel 0
};
class RingBufferImpl : public SOS::Behavior::DummySimpleController<>, private RingBufferTaskImpl {
    public:
    RingBufferImpl(RingBufferBus<RING_BUFFER>& bus) :
    SOS::Behavior::DummySimpleController<>(bus.signal),
    RingBufferTaskImpl(std::get<0>(bus.cables),std::get<0>(bus.const_cables))
    {
        _thread = start(this);
    }
    ~RingBufferImpl() final{
        destroy(_thread);
    }
    void event_loop(){
        //while(is_running()){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                this->read_loop();
            }
            std::this_thread::yield();
        //}
        //finished();
    }
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread;
};