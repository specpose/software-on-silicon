#include "software-on-silicon/RingBuffer.hpp"

using namespace SOS;

class TransferRingToMemory : protected Behavior::RingBufferTask {//, protected Behavior::MemoryControllerWrite {
    public:
    TransferRingToMemory(
        Behavior::RingBufferTask::cable_type& indices,
        Behavior::RingBufferTask::const_cable_type& bounds
        ) : SOS::Behavior::RingBufferTask(indices, bounds){}
    protected:
    virtual void write(const char character) override {}
};
class RingBufferImpl : private SOS::RingBufferLoop, public TransferRingToMemory {
    public:
    RingBufferImpl(MemoryView::RingBufferBus& bus) :
    SOS::RingBufferLoop(bus.signal),
    TransferRingToMemory(std::get<0>(bus.cables),std::get<0>(bus.const_cables))
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
    auto bus = MemoryView::RingBufferBus(hostmemory.begin(),hostmemory.end());
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (buffer!=nullptr)
        delete buffer;
}