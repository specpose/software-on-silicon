#include "software-on-silicon/RingBuffer.hpp"

using namespace SOS::MemoryView;

class MemoryControllerTypeImpl : protected SOS::Behavior::RingBufferTask {//, protected SOS::Behavior::MemoryControllerWrite {
    public:
    using cable_type = std::tuple_element<0,SOS::MemoryView::RingBufferBus::cables_type>::type;
    using const_cable_type = std::tuple_element<0,SOS::MemoryView::RingBufferBus::const_cables_type>::type;
    MemoryControllerTypeImpl(cable_type& indices, const_cable_type& bounds) : SOS::Behavior::RingBufferTask(indices, bounds){}
    protected:
    virtual void write(const char character) override {}
};
class RingBufferImpl : private SOS::RingBufferLoop, public MemoryControllerTypeImpl {
    public:
    RingBufferImpl(RingBufferBus& bus) :
    SOS::RingBufferLoop(bus.signal),
    MemoryControllerTypeImpl(std::get<0>(bus.cables),std::get<0>(bus.const_cables))
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

int main (){
    auto hostmemory = std::array<char,33>{};
    auto bus = RingBufferBus(hostmemory.begin(),hostmemory.end());
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (buffer!=nullptr)
        delete buffer;
}