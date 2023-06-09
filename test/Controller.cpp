#include "software-on-silicon/Controller.hpp"
#include "software-on-silicon/helpers.hpp"
#include <iostream>

using namespace SOS::MemoryView;
using namespace std::chrono;

class DummySubController : public SOS::Behavior::EventLoop<Bus<TypedWire<>,std::array<std::atomic_flag,1>>> {
    public:
    DummySubController(SOS::Behavior::EventLoop<Bus<TypedWire<>,std::array<std::atomic_flag,1>>>::bus_type& bus) :
    SOS::Behavior::EventLoop<Bus<TypedWire<>,std::array<std::atomic_flag,1>>>(bus) {
        std::cout<<"SubController running for 10s..."<<std::endl;
        _thread=start(this);
    }
    ~DummySubController(){
        _thread.join();
        std::cout<<"SubController has ended normally."<<std::endl;
    }
    void event_loop(){
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<10){
            //acquire new data through a wire
            //blink on
            std::get<0>(_intrinsic.signal).test_and_set();
            //run
            operator()();
            //blink off
            std::get<0>(_intrinsic.signal).clear();
            //pause
            std::this_thread::sleep_for(milliseconds{666});
        }
    };
    //SFA::Strict not constexpr
    void operator()(){
        std::this_thread::sleep_for(milliseconds{_duration});
    }
    private:
    std::thread _thread = std::thread{};
    unsigned int _duration = 333;
};

class ControllerImpl : public SOS::Behavior::Controller<
Bus<TypedWire<>,std::array<std::atomic_flag,0>>,
DummySubController
> {
    public:
    ControllerImpl(SOS::Behavior::Controller<Bus<TypedWire<>,std::array<std::atomic_flag,0>>,DummySubController>::bus_type& bus) :
    SOS::Behavior::Controller<
    Bus<TypedWire<>,std::array<std::atomic_flag,0>>,
    DummySubController
    >(bus) {
        _thread=start(this);
    }
    ~ControllerImpl(){
        _thread.join();
    }
    void event_loop(){
        auto waiterSignal = HandShake{};
        auto waiterWire = TypedWire<>{};
        auto waiterBus = make_bus(waiterWire,waiterSignal);
        auto waiter = Timer<milliseconds,100>(waiterBus);

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
        if (std::get<0>(_foreign.signal).test_and_set()) {
            printf("*");
        } else {
            std::get<0>(_foreign.signal).clear();
            printf("_");
        }
    }
    private:
    std::thread _thread = std::thread{};
};

int main () {
    auto mySignal = std::array<std::atomic_flag,0>{};
    auto myWire = TypedWire<>{};
    auto myBus = Bus<decltype(myWire),decltype(mySignal)>{myWire,mySignal};
    ControllerImpl* myController = new ControllerImpl(myBus);
    delete myController;
}