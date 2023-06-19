#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>
#include <chrono>
#include <ratio>

using namespace SOS::MemoryView;

//bus arithmetic_type is not guaranteed to match taskX cable_type
//using buffer_iterator = std::array<double,1000>::iterator;
template<typename ArithmeticType> struct RingBufferBus {
    using signal_type = bus_traits<TaskBus>::signal_type;
    using cables_type = std::tuple< SOS::MemoryView::TaskCable<ArithmeticType, ArithmeticType> >;
    RingBufferBus(ArithmeticType begin, ArithmeticType afterlast) :
    cables(std::tuple<ArithmeticType,ArithmeticType>{begin,begin}),
    start(begin),
    end(afterlast)
    {
        std::cout << "RingBufferBus ArithmeticType is " << typeid(end).name() << std::endl;
        if(std::distance(start,end)<2)
            //should be logic_error
            throw SFA::util::runtime_error("Requested RingBuffer size not big enough.",__FILE__,__func__);
        auto next = start;
        std::get<RingBufferTaskCableWireName::Current>(std::get<0>(cables)).store(++next);
    }
    signal_type signal;
    cables_type cables;
    const ArithmeticType start,end;
};

template<typename ArithmeticType> class RingBufferTask : private SOS::Behavior::Task<ArithmeticType, ArithmeticType, ArithmeticType> {
    public:
    RingBufferTask(SOS::MemoryView::TaskCable<ArithmeticType, ArithmeticType>& indices) :
    SOS::Behavior::Task<ArithmeticType, ArithmeticType, ArithmeticType>(indices),
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
    SOS::MemoryView::TaskCable<ArithmeticType, ArithmeticType>& _item;
};

//OutputBufferType goes into MemoryControllerTask read, write
template<typename OutputBufferType> class RingBufferImpl : private SOS::RingBufferLoop, public RingBufferTask<typename OutputBufferType::iterator> {
    public:
    RingBufferImpl(RingBufferBus<typename OutputBufferType::iterator>& bus) :
    SOS::RingBufferLoop(bus.signal),
    RingBufferTask<typename OutputBufferType::iterator>(std::get<0>(bus.cables)),
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
            if(!get<RingBufferBus<typename OutputBufferType::iterator>::signal_type::Status::notify>(_intrinsic).test_and_set()){
                RingBufferTask<typename OutputBufferType::iterator>::read_loop();
            }
        }
    }

    private:
    bool stop_requested = false;
    //remove BusNotifier template
    typename SOS::MemoryView::BusNotifier::signal_type& _intrinsic;

    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

using namespace std::chrono;

int main(){
    auto hostmemory = std::array<double,1000>{};
    auto bus = RingBufferBus<std::array<double,1000>::iterator>(hostmemory.begin(),hostmemory.end());
    auto memorycontroller = std::array<double,10000>{};
    std::cout << "RingBufferBus current type is" << typeid(std::get<RingBufferTaskCableWireName::Current>(std::get<0>(bus.cables)).load()).name() << std::endl;
    std::cout << "RingBufferBus threadcurrent type is " << typeid(std::get<RingBufferTaskCableWireName::ThreadCurrent>(std::get<0>(bus.cables)).load()).name() << std::endl;
    auto test = bus.start;
    if (test!=std::get<RingBufferTaskCableWireName::ThreadCurrent>(std::get<0>(bus.cables)).load())
        throw SFA::util::runtime_error("Initialization Error",__FILE__,__func__);
    if (++test!=get<RingBufferTaskCableWireName::Current>(std::get<0>(bus.cables)).load())
        throw SFA::util::runtime_error("Initialization Error",__FILE__,__func__);
    RingBufferImpl<std::array<double,10000>>* buffer = new RingBufferImpl<std::array<double,10000>>(bus);
    if (get<RingBufferTaskCableWireName::Current>(std::get<0>(bus.cables)).is_lock_free() &&
    get<RingBufferTaskCableWireName::ThreadCurrent>(std::get<0>(bus.cables)).is_lock_free()){
        try {
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto start = high_resolution_clock::now();
        auto current = get<RingBufferTaskCableWireName::Current>(std::get<0>(bus.cables)).load();
        if (current!=get<RingBufferTaskCableWireName::ThreadCurrent>(std::get<0>(bus.cables)).load()){
            std::cout<<"=";//to be done: write directly
            get<RingBufferTaskCableWireName::Current>(std::get<0>(bus.cables)).store(++current);
            get<RingBufferBus<std::array<double,1000>::iterator>::signal_type::Status::notify>(bus.signal).clear();
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