#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>
#include <chrono>
#include <ratio>

using namespace SOS::MemoryView;

template<typename ArithmeticType> struct RingBufferBus : public TaskBus<ArithmeticType> {
    public:
    RingBufferBus() : TaskBus<ArithmeticType>{} {}
};

//bus arithmetic_type is not guaranteed to match taskX cable_type
using buffer_iterator = std::array<double,1000>::iterator;
template<> struct RingBufferBus< buffer_iterator > : public TaskBus<buffer_iterator> {
    public:
    RingBufferBus() :
    buffer(std::array<double,1000>{}),
    task1(RingBufferTaskCable<buffer_iterator>(buffer.begin(),buffer.begin(),buffer.begin(),buffer.end()))
    {
        if(std::distance(task1.start,task1.end)<2)
            //should be logic_error
            throw SFA::util::runtime_error("Requested RingBuffer size not big enough.",__FILE__,__func__);
        auto next = task1.start;
        get<RingBufferTaskCableWireName::Current>(task1).store(++next);
    }
    RingBufferTaskCable<buffer_iterator> task1;
    private:
    std::array<double,1000> buffer;
};

class RingBufferTask : private SOS::Behavior::Task<buffer_iterator,buffer_iterator> {
    public:
    RingBufferTask(RingBufferTaskCable<buffer_iterator>& indices) : 
    SOS::Behavior::Task<buffer_iterator,buffer_iterator>(indices),
    _item(indices)
    {}
    void read_loop() {
        auto threadcurrent = get<RingBufferTaskCableWireName::ThreadCurrent>(_item).load();
        auto current = get<RingBufferTaskCableWireName::Current>(_item).load();
        auto counter=0;
        while(!(++threadcurrent==current)){
            std::cout<<"+";//to be done: read
            //counter++;
            get<RingBufferTaskCableWireName::ThreadCurrent>(_item).store(threadcurrent);
        }
        //std::cout<<counter;
    }
    private:
    RingBufferTaskCable<buffer_iterator>& _item;
};

class RingBufferImpl : private SOS::RingBufferLoop<buffer_iterator>, private RingBufferTask {
    public:
    using arithmetic_type = SOS::RingBufferLoop<buffer_iterator>::arithmetic_type;
    RingBufferImpl(RingBufferBus<arithmetic_type>& bus) :
    SOS::RingBufferLoop<arithmetic_type>(bus.signal),
    RingBufferTask(bus.task1),
    _intrinsic(bus.signal)
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
            if(!get<RingBufferBus<buffer_iterator>::signal_type::Status::notify>(_intrinsic).test_and_set()){
                read_loop();
            }
        }
    }

    private:
    bool stop_requested = false;
    SOS::MemoryView::BusNotifier<RingBufferBus<buffer_iterator>>::signal_type& _intrinsic;

    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

using namespace std::chrono;

int main(){
    auto bus = RingBufferBus<buffer_iterator>();
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    auto test = bus.task1.start;
    if (test!=get<RingBufferTaskCableWireName::ThreadCurrent>(bus.task1).load())
        throw SFA::util::runtime_error("Initialization Error",__FILE__,__func__);
    if (++test!=get<RingBufferTaskCableWireName::Current>(bus.task1).load())
        throw SFA::util::runtime_error("Initialization Error",__FILE__,__func__);
    if (get<RingBufferTaskCableWireName::Current>(bus.task1).is_lock_free() &&
    get<RingBufferTaskCableWireName::ThreadCurrent>(bus.task1).is_lock_free()){
        try {
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto start = high_resolution_clock::now();
        auto current = get<RingBufferTaskCableWireName::Current>(bus.task1).load();
        if (current!=get<RingBufferTaskCableWireName::ThreadCurrent>(bus.task1).load()){
            std::cout<<"=";//to be done: write directly
            get<RingBufferTaskCableWireName::Current>(bus.task1).store(++current);
            get<RingBufferBus<buffer_iterator>::signal_type::Status::notify>(bus.signal).clear();
        } else {
            //get<RingBufferTaskCableWireName::Current>(bus.task1).store(current);
            std::cout<<std::endl;
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
        }
        std::cout<<std::endl;
        } catch (std::exception& e) {
            delete buffer;
            buffer = nullptr;
        }
        if (buffer!=nullptr)
            delete buffer;
    }
}