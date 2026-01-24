#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/simulation_helpers.hpp"

using namespace SOS::MemoryView;
using namespace std::chrono;

//needs a bus because it is a subcontroller
class SubControllerImpl : public SOS::Behavior::DummySimpleController<> {
    public:
    using bus_type = SOS::MemoryView::BusNotifier;
    SubControllerImpl(bus_type& bus) :
    SOS::Behavior::DummySimpleController<>(bus.signal) {
        std::cout<<"SubController running for 10s..."<<std::endl;
        _thread=start(this);
    }
    ~SubControllerImpl() final {
        destroy(_thread);
        std::cout<<"SubController has ended normally."<<std::endl;
    }
    void event_loop(){
            //would: acquire new data through a wire
            //blink on
            _intrinsic.getNotifyRef().clear();
            //run
            operator()();
            //blink off
            _intrinsic.getNotifyRef().test_and_set();
            //pause
            std::this_thread::sleep_for(milliseconds{666});
    };
    void operator()(){
        std::this_thread::sleep_for(milliseconds{_duration});
    }
    private:
    unsigned int _duration = 333;

    std::thread _thread = std::thread{};
};

//A RunLoop is not a Loop, because it does not have a signal
class ControllerImpl : public SOS::Behavior::BootstrapSimpleController<SubControllerImpl> {
    public:
    ControllerImpl(bus_type& bus) : BootstrapSimpleController<SubControllerImpl>(bus.signal)
    , waiter(new Timer<milliseconds,100>(waiterBus.signal))
    {
        _thread=start(this);
    }
    ~ControllerImpl() {
        destroy(_thread);
        std::cout<<std::endl<<"Controller loop has terminated."<<std::endl;
        delete waiter;
    }
    void event_loop(){
            waiterBus.signal.getUpdatedRef().clear();
            if (!waiterBus.signal.getAcknowledgeRef().test_and_set()){
                if (!_intrinsic.getNotifyRef().test_and_set())
                    request_stop();
                else
                    operator()();
            }
            std::this_thread::yield();
    }
    void operator()(){
        if (!_foreign.signal.getNotifyRef().test_and_set()) {
            _foreign.signal.getNotifyRef().clear();
            std::cout<<"*";
        } else {
            std::cout<<"_";
        }
    }
    private:
    SOS::MemoryView::BusShaker waiterBus{};
    Timer<milliseconds,100>* waiter;
    std::thread _thread = std::thread{};
};
