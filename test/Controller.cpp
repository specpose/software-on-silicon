#include "software-on-silicon/Controller.hpp"
#include "software-on-silicon/helpers.hpp"
#include <iostream>

using namespace SOS::MemoryView;
using namespace std::chrono;

class DummySubController : public SOS::Behavior::SimpleLoop<SOS::Behavior::SubController> {
    public:
    DummySubController(BusNotifier& bus) :
    SOS::Behavior::SimpleLoop<SOS::Behavior::SubController>(bus.signal) {
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
            get<BusNotifier::signal_type::signal::notify>(_intrinsic).clear();
            //run
            operator()();
            //blink off
            get<BusNotifier::signal_type::signal::notify>(_intrinsic).test_and_set();
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

    std::thread _thread = std::thread{};
};

class ControllerImpl : public SOS::Behavior::RunLoop<DummySubController> {
    public:
    ControllerImpl() : SOS::Behavior::RunLoop<DummySubController>() {
        _thread=start(this);
    }
    ~ControllerImpl() final {
        _thread.join();
    }
    void event_loop(){
        auto waiterBus = SOS::MemoryView::BusShaker{};
        auto waiter = Timer<milliseconds,100>(waiterBus.signal);

        std::cout<<"Controller loop running for 5s..."<<std::endl;
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<5){
            get<HandShake::signal::updated>(waiterBus.signal).clear();
            if (!get<HandShake::signal::acknowledge>(waiterBus.signal).test_and_set()){
                operator()();
            }
            std::this_thread::yield();
        }
        std::cout<<std::endl<<"Controller loop has terminated."<<std::endl;
    }
    //SFA::Strict not constexpr
    void operator()(){
        //Note: myBus.signal updated is not used in this example
        if (!get<subcontroller_type::bus_type::signal_type::signal::notify>(_foreign.signal).test_and_set()) {
            get<subcontroller_type::bus_type::signal_type::signal::notify>(_foreign.signal).clear();
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