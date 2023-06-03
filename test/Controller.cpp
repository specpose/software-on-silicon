#include "software-on-silicon/Controller.hpp"
#include <iostream>

using namespace std::chrono;

class SignalsImpl : public SOS::MemoryView::Signals<1> {
    public:
    enum {
        blink
    };
};

static bool get(SignalsImpl& mySignals) {
    auto stateQuery = std::get<SignalsImpl::blink>(mySignals).test_and_set();
    if (!stateQuery)
        std::get<SignalsImpl::blink>(mySignals).clear();
    return stateQuery;
}

class DummySubController : public SOS::Behavior::EventLoop<SignalsImpl> {
    public:
    DummySubController(SOS::Behavior::EventLoop<SignalsImpl>::SignalType& signalbus) : SOS::Behavior::EventLoop<SignalsImpl>(signalbus) {
        std::cout<<"Single Wire received from Controller "<<get(_intrinsic)<<std::endl;
        std::cout<<"Single Wire before EventLoop start "<<get(_intrinsic)<<std::endl;
        _thread=start(this);
        std::cout<<"Single Wire after EventLoop start "<<get(_intrinsic)<<std::endl;
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
            std::cout << message <<std::endl;
            std::this_thread::sleep_for(milliseconds{800});
            //blink off
            std::get<SignalsImpl::blink>(this->_intrinsic).clear();
            //pause
            std::this_thread::sleep_for(milliseconds{1200});
        }
    };
    private:
    std::thread _thread = std::thread{};
    std::string message = std::string{"Thread is (still) running."};
};

class ControllerImpl : public SOS::Behavior::Controller<
SOS::MemoryView::Signals<0>,
DummySubController
> {
    public:
    ControllerImpl(SOS::Behavior::Controller<SOS::MemoryView::Signals<0>,DummySubController>::SignalType& signalbus) : 
    SOS::Behavior::Controller<
    SOS::MemoryView::Signals<0>,
    DummySubController
    >(signalbus) {
        _thread=start(this);
    }
    ~ControllerImpl(){
        std::cout<<"Single Wire before EventLoop teardown "<<get(_wires)<<std::endl;
        _thread.join();
        std::cout<<"Single Wire after EventLoop teardown "<<get(_wires)<<std::endl;
    }
    void event_loop(){
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<3){
            //read as fast as possible to test atomic
            get(_wires);
            /*if(get(_wires)==true)
                std::cout<<"*";
            else
                std::cout<<"_";*/
        }
        //std::cout<<std::endl;
    }
    private:
    std::thread _thread = std::thread{};
};

int main () {
    auto mySignals = SOS::MemoryView::Signals<0>{};
    ControllerImpl* myController = new ControllerImpl(mySignals);
    const auto start = high_resolution_clock::now();
    delete myController;
}