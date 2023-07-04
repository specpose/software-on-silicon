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
        auto threadcurrent = get<cable_type::cable_arithmetic, cable_type::wire_names::ThreadCurrent>(_item).load();
        auto current = get<cable_type::cable_arithmetic, cable_type::wire_names::Current>(_item).load();
        auto counter=0;
        while(!(++threadcurrent==current)){
            std::cout<<"+";//to be done: read
            //counter++;
            get<cable_type::cable_arithmetic, cable_type::wire_names::ThreadCurrent>(_item).store(threadcurrent);
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
    //auto test = bus.start;
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::Current>(std::get<0>(bus.cables)).is_lock_free() &&
    get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).is_lock_free()){
        //try {
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto beginning = high_resolution_clock::now();
        auto current = get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::Current>(std::get<0>(bus.cables)).load();
        const auto start = get<h_mem_iter,WriteSize<h_mem_iter>::wire_names::startPos>(std::get<0>(bus.const_cables));
        const auto end = get<h_mem_iter,WriteSize<h_mem_iter>::wire_names::afterLast>(std::get<0>(bus.const_cables));
        const auto writeLength = get<h_mem_diff,WriteLength<h_mem_diff>::wire_names::WriteLength>(std::get<1>(bus.cables)).load();
        //std::cout << "Current is "<<(void*)current<<" start is "<<(void*)start<<" end is "<<(void*)end<<std::endl;
        if (writeLength>=std::distance(current,end)+std::distance(start,current)){
            throw SFA::util::runtime_error("Individual write length too big or RingBuffer too small",__FILE__,__func__);
        }
        for (h_mem_diff i= 0; i<writeLength;i++){
        if (current!=get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).load()){
            std::cout<<"=";//to be done: write directly
            get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::Current>(std::get<0>(bus.cables)).store(++current);
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