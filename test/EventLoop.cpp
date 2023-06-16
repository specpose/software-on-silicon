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

class BlinkLoop : public SOS::Behavior::SimpleLoop {
    public:
    BlinkLoop(BusNotifier<BlinkLoop>::signal_type& bussignal) :
    SOS::Behavior::SimpleLoop(bussignal), _intrinsic(bussignal) {
        _thread=start(this);
    }
    ~BlinkLoop() final {
        _thread.join();
        std::cout<<"Thread has ended normally."<<std::endl;
    }
    void event_loop(){
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<10){
            //acquire new data through a wire
            //blink on
            get<BusNotifier<BlinkLoop>::signal_type::Status::notify>(_intrinsic).clear();
            //run
            operator()();
            //blink off
            get<BusNotifier<BlinkLoop>::signal_type::Status::notify>(_intrinsic).test_and_set();
            //pause
            std::this_thread::sleep_for(milliseconds{666});
        }
    }
    void operator()(){
        predicate();
    }
    private:
    MyFunctor predicate = MyFunctor();

    typename BusNotifier<BlinkLoop>::signal_type& _intrinsic;
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

int main () {
    auto waiterBus = TimerBus{};
    auto waiter = Timer<milliseconds,100>(waiterBus.signal);

    auto myBus = BusNotifier<BlinkLoop>{};
    std::cout<<"Thread running for 10s..."<<std::endl;
    BlinkLoop* myHandler = new BlinkLoop(myBus.signal);
    std::cout<<"main() loop running for 5s..."<<std::endl;
    const auto start = high_resolution_clock::now();
    while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<5){
        get<HandShake::Status::updated>(waiterBus.signal).test_and_set();
        if (get<HandShake::Status::ack>(waiterBus.signal).test_and_set()){
            get<HandShake::Status::ack>(waiterBus.signal).clear();
            if (!get<BusNotifier<BlinkLoop>::signal_type::Status::notify>(myBus.signal).test_and_set()) {
                get<BusNotifier<BlinkLoop>::signal_type::Status::notify>(myBus.signal).clear();
                printf("*");
            } else {
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