#include "software-on-silicon/error.hpp"
#include "software-on-silicon/INTERFACE.hpp"
#include <iostream>
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
    }
    void event_loop(){
        while(is_running()){
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
        finished();
        std::cout<<"SubController has ended normally."<<std::endl;
    };
    void operator()(){
        std::this_thread::sleep_for(milliseconds{_duration});
    }
    private:
    unsigned int _duration = 333;

    std::thread _thread = std::thread{};
};

//A RunLoop is not a Loop, because it does not have a signal
class ControllerImpl : public SOS::Behavior::SimpleController<SubControllerImpl> {
    public:
    ControllerImpl(bus_type& bus) : SimpleController<SubControllerImpl>(bus.signal) {
        _thread=start(this);
    }
    ~ControllerImpl() {
        destroy(_thread);
    }
    void event_loop(){
        auto waiterBus = SOS::MemoryView::BusShaker{};
        auto waiter = Timer<milliseconds,100>(waiterBus.signal);

        bool stopme = false;
        while(is_running() && !stopme){
            waiterBus.signal.getUpdatedRef().clear();
            if (!waiterBus.signal.getAcknowledgeRef().test_and_set()){
                if (!_intrinsic.getNotifyRef().test_and_set())
                    stopme = true;
                else
                    operator()();
            }
            std::this_thread::yield();
        }
        finished();
        std::cout<<std::endl<<"Controller loop has terminated."<<std::endl;
        waiter.requestStop();
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
    std::thread _thread = std::thread{};
};
