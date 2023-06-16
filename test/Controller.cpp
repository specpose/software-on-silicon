#include "software-on-silicon/Controller.hpp"
#include "software-on-silicon/helpers.hpp"
#include <iostream>

using namespace SOS::MemoryView;
using namespace std::chrono;

class DummySubController : public SOS::Behavior::SimpleLoop {
    public:
    DummySubController(BusNotifier<DummySubController>& bus) :
    SOS::Behavior::SimpleLoop(bus.signal), _intrinsic(bus.signal) {
        std::cout<<"SubController running for 10s..."<<std::endl;
        _thread=start(this);
    }
    ~DummySubController() final {
        _thread.join();
        std::cout<<"SubController has ended normally."<<std::endl;
    }
    void event_loop(){
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<10){
            //acquire new data through a wire
            //blink on
            get<BusNotifier<DummySubController>::signal_type::Status::notify>(_intrinsic).clear();
            //run
            operator()();
            //blink off
            get<BusNotifier<DummySubController>::signal_type::Status::notify>(_intrinsic).test_and_set();
            //pause
            std::this_thread::sleep_for(milliseconds{666});
        }
    };
    //SFA::Strict not constexpr
    void operator()(){
        std::this_thread::sleep_for(milliseconds{_duration});
    }
    private:
    unsigned int _duration = 333;

    BusNotifier<DummySubController>::signal_type& _intrinsic;
    std::thread _thread = std::thread{};
};

class ControllerImpl : public SOS::Behavior::Controller<DummySubController> {
    public:
    ControllerImpl() : SOS::Behavior::Controller<DummySubController>() {
        _thread=start(this);
    }
    ~ControllerImpl() final {
        _thread.join();
    }
    void event_loop(){
        auto waiterBus = TimerBus{};
        auto waiter = Timer<milliseconds,100>(waiterBus.signal);

        std::cout<<"Controller loop running for 5s..."<<std::endl;
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<5){
            get<HandShake::Status::updated>(waiterBus.signal).test_and_set();
            if (get<HandShake::Status::ack>(waiterBus.signal).test_and_set()){
                get<HandShake::Status::ack>(waiterBus.signal).clear();
                operator()();
            } else {
                get<HandShake::Status::ack>(waiterBus.signal).clear();
            }
            std::this_thread::yield();
        }
        std::cout<<std::endl<<"Controller loop has terminated."<<std::endl;
    }
    //SFA::Strict not constexpr
    void operator()(){
        //Note: myBus.signal updated is not used in this example
        if (!get<subcontroller_type::signal_type::Status::notify>(_foreign.signal).test_and_set()) {
            get<subcontroller_type::signal_type::Status::notify>(_foreign.signal).clear();
            printf("*");
        } else {
            printf("_");
        }
    }
    private:
    std::thread _thread = std::thread{};
};

int main () {
    ControllerImpl* myController = new ControllerImpl();
    delete myController;
}