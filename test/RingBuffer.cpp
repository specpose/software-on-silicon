#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>
#include <chrono>
#include <ratio>

using namespace SOS::MemoryView;

struct RingBufferBus {
    using signal_type = bus_traits<TaskBus>::signal_type;
    using _arithmetic_type = std::array<double,0>::iterator;
    using cables_type = std::tuple< SOS::MemoryView::RingBufferTaskCable<_arithmetic_type> >;
    RingBufferBus(_arithmetic_type begin, _arithmetic_type afterlast) :
    //tuple requires copy constructor for any tuple that isn't default constructed
    cables{std::tuple< RingBufferTaskCable<_arithmetic_type> >{}
    },
    start(begin),
    end(afterlast)
    {
        //explicitly initialize wires
        std::get<RingBufferTaskCable<_arithmetic_type>::wire_names::ThreadCurrent>(std::get<0>(cables)).store(begin);
        std::cout << "RingBufferBus ArithmeticType is " << typeid(end).name() << std::endl;
        if(std::distance(start,end)<2)
            //should be logic_error
            throw SFA::util::runtime_error("Requested RingBuffer size not big enough.",__FILE__,__func__);
        auto next = start;
        std::get<RingBufferTaskCable<_arithmetic_type>::wire_names::Current>(std::get<0>(cables)).store(++next);
    }
    signal_type signal;
    cables_type cables;
    const _arithmetic_type start,end;
};

class RingBufferTask;

/*template<> struct SOS::Behavior::task_traits<RingBufferTask> {
    using cable_type = typename SOS::MemoryView::RingBufferTaskCable<std::array<double,1000>::iterator>;
};*/

class RingBufferTask {//: private SOS::Behavior::Task {
    public:
    using cable_type = std::tuple_element<0,RingBufferBus::cables_type>::type;
    RingBufferTask(cable_type& indices) :
    //SOS::Behavior::Task(indices),
    _item(indices)
    {}
    void read_loop() {
        auto threadcurrent = std::get<cable_type::wire_names::ThreadCurrent>(_item).load();
        auto current = std::get<cable_type::wire_names::Current>(_item).load();
        auto counter=0;
        while(!(++threadcurrent==current)){
            std::cout<<"+";//to be done: read
            //counter++;
            std::get<cable_type::wire_names::ThreadCurrent>(_item).store(threadcurrent);
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
            if(!get<RingBufferBus::signal_type::Status::notify>(_intrinsic).test_and_set()){
                RingBufferTask::read_loop();
            }
        }
    }

    private:
    std::array<double,10000> memorycontroller = std::array<double,10000>{};
    bool stop_requested = false;

    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

using namespace std::chrono;

int main(){
    auto hostmemory = std::array<double,1000>{};
    auto bus = RingBufferBus(hostmemory.begin(),hostmemory.end());
    std::cout << "RingBufferBus current type is" << typeid(std::get<RingBufferTaskCable<std::array<double,1000>::iterator>::wire_names::Current>(std::get<0>(bus.cables)).load()).name() << std::endl;
    std::cout << "RingBufferBus threadcurrent type is " << typeid(std::get<RingBufferTaskCable<std::array<double,1000>::iterator>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).load()).name() << std::endl;
    auto test = bus.start;
    if (test!=std::get<RingBufferTaskCable<std::array<double,1000>::iterator>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).load())
        throw SFA::util::runtime_error("Initialization Error",__FILE__,__func__);
    if (++test!=std::get<RingBufferTaskCable<std::array<double,1000>::iterator>::wire_names::Current>(std::get<0>(bus.cables)).load())
        throw SFA::util::runtime_error("Initialization Error",__FILE__,__func__);
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (std::get<RingBufferTaskCable<std::array<double,1000>::iterator>::wire_names::Current>(std::get<0>(bus.cables)).is_lock_free() &&
    std::get<RingBufferTaskCable<std::array<double,1000>::iterator>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).is_lock_free()){
        try {
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto start = high_resolution_clock::now();
        auto current = std::get<RingBufferTaskCable<std::array<double,1000>::iterator>::wire_names::Current>(std::get<0>(bus.cables)).load();
        if (current!=std::get<RingBufferTaskCable<std::array<double,1000>::iterator>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).load()){
            std::cout<<"=";//to be done: write directly
            std::get<RingBufferTaskCable<std::array<double,1000>::iterator>::wire_names::Current>(std::get<0>(bus.cables)).store(++current);
            get<RingBufferBus::signal_type::Status::notify>(bus.signal).clear();
        } else {
            //get<RingBufferTaskCableWireName::Current>(std::get<0>(bus.cables)).store(current);
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