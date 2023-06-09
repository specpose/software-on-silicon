#include "software-on-silicon/helpers.hpp"
#include <iostream>

using namespace SOS::MemoryView;
using namespace std::chrono;

class MyFunctor {
    public:
    void operator()(){
        std::this_thread::sleep_for(milliseconds{_duration});
    }
    private:
    unsigned int _duration = 333;
};

class BlinkLoop : public SOS::Behavior::EventLoop<Bus<TypedWire<>,std::array<std::atomic_flag,1>>> {
    public:
    BlinkLoop(SOS::Behavior::EventLoop<Bus<TypedWire<>,std::array<std::atomic_flag,1>>>::bus_type& signalbus) :
    SOS::Behavior::EventLoop<Bus<TypedWire<>,std::array<std::atomic_flag,1>>>(signalbus) {
        _thread=start(this);
    }
    ~BlinkLoop(){
        _thread.join();
        std::cout<<"Thread has ended normally."<<std::endl;
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
    auto waiterSignal = HandShake{};
    auto waiterWire = TypedWire<>{};
    auto waiterBus = make_bus(waiterWire,waiterSignal);
    auto waiter = Timer<milliseconds,100>(waiterBus);

    auto mySignal = std::array<std::atomic_flag,1>{};
    auto myWire = TypedWire<>{};
    auto myBus = Bus<decltype(myWire),decltype(mySignal)>{myWire,mySignal};
    std::cout<<"Thread running for 10s..."<<std::endl;
    BlinkLoop* myHandler = new BlinkLoop(myBus);
    std::cout<<"main() loop running for 5s..."<<std::endl;
    const auto start = high_resolution_clock::now();
    while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<5){
        get<HandShake::Status::updated>(waiterBus.signal).test_and_set();
        if (get<HandShake::Status::ack>(waiterBus.signal).test_and_set()){
            get<HandShake::Status::ack>(waiterBus.signal).clear();
            //Note: myBus.signal updated is not used in this example
            if (std::get<0>(myBus.signal).test_and_set()) {
                printf("*");
            } else {
                std::get<0>(myBus.signal).clear();
                printf("_");
            }
        } else {
            get<HandShake::Status::ack>(waiterBus.signal).clear();
        }
        std::this_thread::yield();
    }
    std::cout<<std::endl<<"main() loop has terminated."<<std::endl;
    delete myHandler;
}