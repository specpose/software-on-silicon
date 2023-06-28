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
        get<_arithmetic_type,RingBufferTaskCable<_arithmetic_type>::wire_names::ThreadCurrent>(std::get<0>(cables)).store(begin);
        std::cout << "RingBufferBus ArithmeticType is " << typeid(end).name() << std::endl;
        if(std::distance(start,end)<2)
            //should be logic_error
            throw SFA::util::runtime_error("Requested RingBuffer size not big enough.",__FILE__,__func__);
        auto next = start;
        get<_arithmetic_type,RingBufferTaskCable<_arithmetic_type>::wire_names::Current>(std::get<0>(cables)).store(++next);
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
            if(!get<RingBufferBus::signal_type::signal::notify>(_intrinsic).test_and_set()){
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
    using h_mem_iter = decltype(hostmemory)::iterator;
    auto bus = RingBufferBus(hostmemory.begin(),hostmemory.end());
    std::cout << "RingBufferBus current type is" << typeid(get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::Current>(std::get<0>(bus.cables)).load()).name() << std::endl;
    std::cout << "RingBufferBus threadcurrent type is " << typeid(get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).load()).name() << std::endl;
    auto test = bus.start;
    if (test!=get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).load())
        throw SFA::util::runtime_error("Initialization Error",__FILE__,__func__);
    if (++test!=get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::Current>(std::get<0>(bus.cables)).load())
        throw SFA::util::runtime_error("Initialization Error",__FILE__,__func__);
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::Current>(std::get<0>(bus.cables)).is_lock_free() &&
    get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).is_lock_free()){
        try {
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto start = high_resolution_clock::now();
        auto current = get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::Current>(std::get<0>(bus.cables)).load();
        if (current!=get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::ThreadCurrent>(std::get<0>(bus.cables)).load()){
            std::cout<<"=";//to be done: write directly
            get<h_mem_iter,RingBufferTaskCable<h_mem_iter>::wire_names::Current>(std::get<0>(bus.cables)).store(++current);
            get<RingBufferBus::signal_type::signal::notify>(bus.signal).clear();
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