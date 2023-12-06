#include "software-on-silicon/error.hpp"
#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"

using namespace SOS::MemoryView;
using namespace std::chrono;

//needs a bus because it is a subcontroller
class SubControllerImpl : public SOS::Behavior::DummyController<SOS::MemoryView::Notify>, public SOS::Behavior::Loop {
    public:
    using bus_type = SOS::MemoryView::BusNotifier;
    SubControllerImpl(bus_type& bus) :
    SOS::Behavior::DummyController<SOS::MemoryView::Notify>(bus.signal),
    SOS::Behavior::Loop() {
        std::cout<<"SubController running for 10s..."<<std::endl;
        _thread=start(this);
    }
    ~SubControllerImpl() final {
        _thread.join();
        std::cout<<"SubController has ended normally."<<std::endl;
    }
    void event_loop(){
        while(stop_token.getUpdatedRef().test_and_set()){
            //would: acquire new data through a wire
            //blink on
            _intrinsic.getNotifyRef().clear();
            //run
            operator()();
            //blink off
            _intrinsic.getNotifyRef().test_and_set();
            //pause
            std::this_thread::sleep_for(milliseconds{666});
        }
        stop_token.getAcknowledgeRef().clear();
    };
    void operator()(){
        std::this_thread::sleep_for(milliseconds{_duration});
    }
    private:
    unsigned int _duration = 333;

    std::thread _thread = std::thread{};
};

//A RunLoop is not a Loop, because it does not have a signal
class ControllerImpl : private Thread<SubControllerImpl>, public SOS::Behavior::Loop {
    public:
    ControllerImpl() : Thread<SubControllerImpl>(), Loop() {
        _thread=start(this);
    }
    ~ControllerImpl() {
        _child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        _thread.join();
    }
    void event_loop(){
        auto waiterBus = SOS::MemoryView::BusShaker{};
        auto waiter = Timer<milliseconds,100>(waiterBus.signal);

        while(stop_token.getUpdatedRef().test_and_set()){
            waiterBus.signal.getUpdatedRef().clear();
            if (!waiterBus.signal.getAcknowledgeRef().test_and_set()){
                operator()();
            }
            std::this_thread::yield();
        }
        std::cout<<std::endl<<"Controller loop has terminated."<<std::endl;
        stop_token.getAcknowledgeRef().clear();
    }
    void operator()(){
        if (!_foreign.signal.getNotifyRef().test_and_set()) {
            _foreign.signal.getNotifyRef().clear();
            printf("*");
        } else {
            printf("_");
        }
    }
    private:
    std::thread _thread = std::thread{};
};