#include "software-on-silicon/helpers.hpp"
#include <iostream>
#include <chrono>

using namespace SOS::MemoryView;
/*class MemoryControllerWriteDummy {
    protected:
    //should be private
    void write(const char character) {std::cout<<character;}
};*/
class RingBufferTaskImpl : protected SOS::Behavior::RingBufferTask {
    public:
    using cable_type = std::tuple_element<0,SOS::MemoryView::RingBufferBus::cables_type>::type;
    using const_cable_type = std::tuple_element<0,SOS::MemoryView::RingBufferBus::const_cables_type>::type;
    RingBufferTaskImpl(cable_type& indices, const_cable_type& bounds) : SOS::Behavior::RingBufferTask(indices, bounds){}
    protected:
    virtual void write(const char character) override {std::cout<<character;}
};
class RingBufferImpl : private SOS::RingBufferLoop, public RingBufferTaskImpl {
    public:
    RingBufferImpl(RingBufferBus& bus) :
    SOS::RingBufferLoop(bus.signal),
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

using namespace std::chrono;

int main(){
    auto hostmemory = std::array<char,33>{};
    auto bus = RingBufferBus(hostmemory.begin(),hostmemory.end());
    auto hostwriter = PieceWriter<decltype(hostmemory)>(bus);
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
    const auto beginning = high_resolution_clock::now();
    //try {
    hostwriter.writePiece(0,32);
    std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
    }
    std::cout<<std::endl;
    /*} catch (std::exception& e) {
        delete buffer;
        buffer = nullptr;
    }*/
    if (buffer!=nullptr)
        delete buffer;
}