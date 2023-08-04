#include "software-on-silicon/error.hpp"
#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/RingBuffer.hpp"

namespace SOSFloat {
using SAMPLE_SIZE = float;
using RING_BUFFER=std::array< SOS::MemoryView::Contiguous<SAMPLE_SIZE>*,10 >;

using namespace SOS::MemoryView;

class RingBufferTaskImpl : protected SOS::Behavior::RingBufferTask<RING_BUFFER> {
    public:
    using cable_type = std::tuple_element<0,RingBufferBus<RING_BUFFER>::cables_type>::type;
    using const_cable_type = std::tuple_element<0,RingBufferBus<RING_BUFFER>::const_cables_type>::type;
    RingBufferTaskImpl(cable_type& indices, const_cable_type& bounds) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds){}
    protected:
    virtual void write(const RING_BUFFER::value_type character) final {std::cout<<(*character)[0];}//HACK: hard-coded channel 0
};
class RingBufferImpl : private SOS::Behavior::SimpleController<SOS::Behavior::DummyController>, public RingBufferTaskImpl {
    public:
    RingBufferImpl(RingBufferBus<RING_BUFFER>& bus) :
    SOS::Behavior::SimpleController<SOS::Behavior::DummyController>(bus.signal),
    RingBufferTaskImpl(std::get<0>(bus.cables),std::get<0>(bus.const_cables))
    {
        _thread = start(this);
    }
    ~RingBufferImpl() final{
        stop_requested=true;
        _thread.join();
    }
    void event_loop(){
        while(!stop_requested){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                RingBufferTask::read_loop();
            }
        }
    }
    private:
    bool stop_requested = false;
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};
}