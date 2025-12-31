#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/RingBuffer.hpp"

typedef char SAMPLE_SIZE;
#define RING_BUFFER std::array<std::array<SAMPLE_SIZE,1>,33>

using namespace SOS::MemoryView;

class RingBufferTaskImpl : protected SOS::Behavior::RingBufferTask<RING_BUFFER> {
    public:
    using cable_type = std::tuple_element<0,RingBufferBus<RING_BUFFER>::cables_type>::type;
    using const_cable_type = std::tuple_element<0,RingBufferBus<RING_BUFFER>::const_cables_type>::type;
    RingBufferTaskImpl(cable_type& indices, const_cable_type& bounds) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds){}
    protected:
    virtual void read_loop() final {
        auto threadcurrent = _item.getThreadCurrentRef().load();
        auto current = _item.getCurrentRef().load();
        bool stop = false;
        while(!stop){//if: possible less writes than reads
            ++threadcurrent;
            if (threadcurrent==_bounds.getWriterEndRef())
                threadcurrent=_bounds.getWriterStartRef();
            if (threadcurrent!=current) {
                write(*threadcurrent);
                _item.getThreadCurrentRef().store(threadcurrent);
            } else {
                stop = true;
            }
        }
    }
    private:
    virtual void write(const RING_BUFFER::value_type character) final {std::cout<<character.at(0);}
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
        while(is_running()){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                this->read_loop();
            }
            std::this_thread::yield();
        }
        finished();
    }
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};
