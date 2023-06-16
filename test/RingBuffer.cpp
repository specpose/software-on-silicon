#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>

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
    task1(RingBufferTaskCable<buffer_iterator>{buffer.begin(),buffer.begin(),buffer.end()})
    {}
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
    void print_status() {
        std::cout<<"Wire Current is "<<get<RingBufferTaskCableWireName::Current>(_item).load()<<std::endl;
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
        _thread.join();
    }
    void event_loop(){
        print_status();
    }

    private:
    SOS::MemoryView::BusNotifier<RingBufferBus<buffer_iterator>>::signal_type& _intrinsic;

    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

int main(){
    const size_t n = 1;
    auto bus = RingBufferBus<buffer_iterator>();
    //auto bus = RingBufferBusImpl{Notify{},std::array<size_t,2>{0,1},RingBufferBus<bool,1000>::data_type(n)};
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    if (get<RingBufferTaskCableWireName::Current>(bus.task1).is_lock_free() &&
    get<RingBufferTaskCableWireName::ThreadCurrent>(bus.task1).is_lock_free()){
        try {
        while (true) {
        std::cout << "Before Update Current is " << get<RingBufferTaskCableWireName::Current>(bus.task1).load() << std::endl;
        std::cout << "Before Update ThreadCurrent is " << get<RingBufferTaskCableWireName::ThreadCurrent>(bus.task1).load() << std::endl;
        auto current = get<RingBufferTaskCableWireName::Current>(bus.task1).load();
        if (current!=get<RingBufferTaskCableWireName::ThreadCurrent>(bus.task1).load()){
            get<RingBufferTaskCableWireName::Current>(bus.task1).store(++current);
            std::cout<<"+";
        } else {
            get<RingBufferTaskCableWireName::Current>(bus.task1).store(++current);
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        //if (!get<RingBufferBus<buffer_iterator>::signal_type::Status::notify>(bus.signal).test_and_set()){
            get<RingBufferBus<buffer_iterator>::signal_type::Status::notify>(bus.signal).clear();
            //std::cout<<"=";
        //}
        std::cout << "After Update Current is " << get<RingBufferTaskCableWireName::Current>(bus.task1).load() << std::endl;
        std::cout << "After Update ThreadCurrent is " << get<RingBufferTaskCableWireName::ThreadCurrent>(bus.task1).load() << std::endl;
        }
        } catch (std::exception& e) {
            delete buffer;
            std::cout << "RingBuffer shutdown"<<std::endl;
        }
    }
}