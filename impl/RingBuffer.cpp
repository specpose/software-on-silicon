#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/RingBuffer.hpp"

#include "Sample.cpp"
using RING_BUFFER=std::array<std::array<SOS::MemoryView::sample<SAMPLE_TYPE,1>,MAX_BLINK>,10>;//100

class RingBufferTaskImpl : private virtual SOS::Behavior::RingBufferTask<RING_BUFFER> {
    public:
    using SOS::Behavior::RingBufferTask<RING_BUFFER>::RingBufferTask;
    protected:
    virtual void transfer(const RING_BUFFER::value_type& character) {std::cout<<character[0][0];}//HACK: hard coded single sample, hard coded channel 0
};
class RingBufferImpl : public SOS::Behavior::RingBuffer<RING_BUFFER>, private RingBufferTaskImpl, public SOS::Behavior::Loop {
    public:
    RingBufferImpl(bus_type& bus) :
    SOS::Behavior::RingBuffer<RING_BUFFER>(bus.signal),
    RingBufferTaskImpl(std::get<0>(bus.cables),std::get<0>(bus.const_cables)),
    SOS::Behavior::RingBufferTask<RING_BUFFER>(std::get<0>(bus.cables),std::get<0>(bus.const_cables)),
    SOS::Behavior::Loop()
    {
        _thread = start(this);
    }
    ~RingBufferImpl() {
        destroy(_thread);
    }
    virtual void event_loop() final {
        SOS::Behavior::RingBuffer<RING_BUFFER>::event_loop();
    }
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread;
};