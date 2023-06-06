#include "software-on-silicon/Controller.hpp"
#include "software-on-silicon/helpers.hpp"
#include <iostream>

using namespace SOS::MemoryView;
using namespace std::chrono;

class DummySubController : public SOS::Behavior::EventLoop<Bus<TypedWire<>>> {
    public:
    DummySubController(SOS::Behavior::EventLoop<Bus<TypedWire<>>>::bus_type& signalbus) : SOS::Behavior::EventLoop<Bus<TypedWire<>>>(signalbus) {
        std::cout<<"Wire received from Controller "<<get<HandShake::Status::updated>(_intrinsic.signal)<<std::endl;
        std::cout<<"Wire before EventLoop start "<<get<HandShake::Status::updated>(_intrinsic.signal)<<std::endl;
        _thread=start(this);
        std::cout<<"Wire after EventLoop start "<<get<HandShake::Status::updated>(_intrinsic.signal)<<std::endl;
    }
    ~DummySubController(){
        _thread.join();
    }
    void event_loop(){
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<10){
            //acquire new data through a wire
            //blink on
            std::get<HandShake::Status::updated>(this->_intrinsic.signal).test_and_set();
            //run
            operator()();
            //blink off
            std::get<HandShake::Status::updated>(this->_intrinsic.signal).clear();
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
Bus<TypedWire<>,std::array<std::atomic_flag,0>>,
DummySubController
> {
    public:
    ControllerImpl(SOS::Behavior::Controller<Bus<TypedWire<>,std::array<std::atomic_flag,0>>,DummySubController>::bus_type& signalbus) : 
    SOS::Behavior::Controller<
    Bus<TypedWire<>,std::array<std::atomic_flag,0>>,
    DummySubController
    >(signalbus) {
        _thread=start(this);
    }
    ~ControllerImpl(){
        std::cout<<"Wire before EventLoop teardown "<<get<HandShake::Status::updated>(_foreign.signal)<<std::endl;
        _thread.join();
        std::cout<<"Wire after Thread join "<<get<HandShake::Status::updated>(_foreign.signal)<<std::endl;
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
            get<HandShake::Status::updated>(_foreign.signal);
            /*if(get<HandShake::Status::updated>(_foreign.signal)==true)
                std::cout<<"*";
            else
                std::cout<<"_";*/
    }
    private:
    std::thread _thread = std::thread{};
};

int main () {
    auto mySignal = std::array<std::atomic_flag,0>{};
    auto myWire = TypedWire<>{};
    auto myBus = Bus<decltype(myWire),decltype(mySignal)>{mySignal,myWire};
    ControllerImpl* myController = new ControllerImpl(myBus);
    delete myController;
}