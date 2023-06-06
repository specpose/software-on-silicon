#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/helpers.hpp"
#include <iostream>

using namespace SOS::MemoryView;
using namespace std::chrono;

class MyFunctor {
    public:
    void operator()(){
        std::cout << message <<std::endl;
        std::this_thread::sleep_for(milliseconds{800});
    }
    private:
    std::string message = std::string{"Thread is (still) running."};
};

class BlinkLoop : public SOS::Behavior::EventLoop<Bus<TypedWire<>>> {
    public:
    BlinkLoop(SOS::Behavior::EventLoop<Bus<TypedWire<>>>::bus_type& signalbus) : SOS::Behavior::EventLoop<Bus<TypedWire<>>>(signalbus) {
        _thread=start(this);
    }
    ~BlinkLoop(){
        _thread.join();
    }
    void event_loop(){
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<10){
            //acquire new data through a wire
            //blink on
            std::get<HandShake::Status::updated>(_intrinsic.signal).test_and_set();
            //run
            operator()();
            //blink off
            std::get<HandShake::Status::updated>(_intrinsic.signal).clear();
            //pause
            std::this_thread::sleep_for(milliseconds{1200});
        }
    }
    void operator()(){
        predicate();
    }
    private:
    MyFunctor predicate = MyFunctor();
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

int main () {
    auto mySignal = HandShake{};
    auto myWire = TypedWire<>{};
    auto myBus = make_bus(mySignal,myWire);
    std::cout<<"Wire initialised to "<<get<HandShake::Status::updated>(myBus.signal)<<std::endl;
    std::cout<<"Wire before EventLoop start "<<get<HandShake::Status::updated>(myBus.signal)<<std::endl;
    BlinkLoop* myHandler = new BlinkLoop(myBus);
    std::cout<<"Wire after EventLoop start "<<get<HandShake::Status::updated>(myBus.signal)<<std::endl;
    const auto start = high_resolution_clock::now();
    while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<3){
        //read as fast as possible to test atomic
        get<HandShake::Status::updated>(myBus.signal);
        /*if(get<HandShake::Status::updated>(myBus.signal)==true)
            std::cout<<"*";
        else
            std::cout<<"_";*/
    }
    //std::cout<<std::endl;
    std::cout<<"Wire before EventLoop teardown "<<get<HandShake::Status::updated>(myBus.signal)<<std::endl;
    delete myHandler;
    std::cout<<"Wire after EventLoop teardown "<<get<HandShake::Status::updated>(myBus.signal)<<std::endl;
}