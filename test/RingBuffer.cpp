#include "software-on-silicon/RingBuffer.hpp"
#include <iostream>
#include <chrono>

using namespace SOS::MemoryView;

class RingBufferTask {
    public:
    using cable_type = std::tuple_element<0,RingBufferBus::cables_type>::type;
    RingBufferTask(cable_type& indices) :
    _item(indices)
    {}
    void read_loop() {
        auto threadcurrent = _item.getThreadCurrentRef().load();
        auto current = _item.getCurrentRef().load();
        auto counter=0;
        while(!(++threadcurrent==current)){
            std::cout<<"+";//to be done: read
            //counter++;
            _item.getThreadCurrentRef().store(threadcurrent);
        }
        //std::cout<<counter;
    }
    private:
    cable_type& _item;
};

class RingBufferImpl : private SOS::RingBufferLoop, public RingBufferTask {
    public:
    RingBufferImpl(RingBufferBus& bus) :
    SOS::RingBufferLoop(bus.signal),
    RingBufferTask(std::get<0>(bus.cables))
    {
        _thread = start(this);
        std::cout<<"RingBuffer started"<<std::endl;
    }
    ~RingBufferImpl() final{
        stop_requested=true;
        _thread.join();
        std::cout<<"RingBuffer shutdown"<<std::endl;
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
    auto hostmemory = std::array<char,1000>{};
    using h_mem_iter = decltype(hostmemory)::iterator;
    using h_mem_diff = decltype(hostmemory)::difference_type;
    auto bus = RingBufferBus(hostmemory.begin(),hostmemory.end());
    bus.setLength(1);
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (std::get<0>(bus.cables).getCurrentRef().is_lock_free() && std::get<0>(bus.cables).getThreadCurrentRef().is_lock_free()){
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto beginning = high_resolution_clock::now();
        auto current = std::get<0>(bus.cables).getCurrentRef().load();
        const auto start = std::get<0>(bus.const_cables).getWriterStartRef();
        const auto end = std::get<0>(bus.const_cables).getWriterEndRef();
        const auto writeLength = std::get<1>(bus.cables).getWriteLengthRef().load();
        //try {
        if (writeLength>=std::distance(current,end)+std::distance(start,current)){
            throw SFA::util::runtime_error("Individual write length too big or RingBuffer too small",__FILE__,__func__);
        }
        for (h_mem_diff i= 0; i<writeLength;i++){
        if (current!=std::get<0>(bus.cables).getThreadCurrentRef().load()){
            std::cout<<"=";//to be done: write directly
            std::get<0>(bus.cables).getCurrentRef().store(++current);
            bus.signal.getNotifyRef().clear();
        } else {
            //get<RingBufferTaskCableWireName::Current>(std::get<0>(bus.cables)).store(current);
            std::cout<<std::endl;
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        }
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
}