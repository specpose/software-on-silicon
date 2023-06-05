#include "software-on-silicon/Controller.hpp"
#include "software-on-silicon/helpers.hpp"
#include <iostream>

using namespace SOS::MemoryView;
using namespace std::chrono;

class DummySubController : public SOS::Behavior::EventLoop<SignalsImpl>, private SFA::Lazy {
    public:
    DummySubController(SOS::Behavior::EventLoop<SignalsImpl>::SignalType& signalbus) : SOS::Behavior::EventLoop<SignalsImpl>(signalbus) {
        std::cout<<"Wire received from Controller "<<get<SignalsImpl::blink>(_intrinsic)<<std::endl;
        std::cout<<"Wire before EventLoop start "<<get<SignalsImpl::blink>(_intrinsic)<<std::endl;
        _thread=start(this);
        std::cout<<"Wire after EventLoop start "<<get<SignalsImpl::blink>(_intrinsic)<<std::endl;
    }
    ~DummySubController(){
        _thread.join();
    }
    void event_loop(){
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<10){
            //acquire new data through a wire
            //blink on
            std::get<SignalsImpl::blink>(this->_intrinsic).test_and_set();
            //run
            operator()();
            //blink off
            std::get<SignalsImpl::blink>(this->_intrinsic).clear();
            //pause
            std::this_thread::sleep_for(milliseconds{1200});
        }
    };
    //SFA::Strict not constexpr
    void operator()(){
        std::cout << message <<std::endl;
        std::this_thread::sleep_for(milliseconds{800});
    }
    private:
    std::thread _thread = std::thread{};
    std::string message = std::string{"Thread is (still) running."};
};

class ControllerImpl : public SOS::Behavior::Controller<
Signals<0>,
DummySubController
>, private SFA::Lazy {
    public:
    ControllerImpl(SOS::Behavior::Controller<Signals<0>,DummySubController>::SignalType& signalbus) : 
    SOS::Behavior::Controller<
    Signals<0>,
    DummySubController
    >(signalbus) {
        _thread=start(this);
    }
    ~ControllerImpl(){
        std::cout<<"Wire before EventLoop teardown "<<get<SOS::Behavior::Controller<Signals<0>,DummySubController>::SubControllerType::SignalType::blink>(_wires)<<std::endl;
        _thread.join();
        std::cout<<"Wire after Thread join "<<get<SOS::Behavior::Controller<Signals<0>,DummySubController>::SubControllerType::SignalType::blink>(_wires)<<std::endl;
    }
    void event_loop(){
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<3){
            operator()();
        }
        //std::cout<<std::endl;
    }
    //SFA::Strict not constexpr
    void operator()(){
            //read as fast as possible to test atomic
            get<SOS::Behavior::Controller<Signals<0>,DummySubController>::SubControllerType::SignalType::blink>(_wires);
            /*if(get(_wires)==true)
                std::cout<<"*";
            else
                std::cout<<"_";*/
    }
    private:
    std::thread _thread = std::thread{};
};

int main () {
    auto mySignals = Signals<0>{};
    ControllerImpl* myController = new ControllerImpl(mySignals);
    const auto start = high_resolution_clock::now();
    delete myController;
}