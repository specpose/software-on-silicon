#include "software-on-silicon/EventLoop.hpp"
#include <iostream>

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

class SignalsImpl : public SOS::MemoryView::Signals<1> {
    public:
    enum {
        blink
    };
};

static bool get(SOS::MemoryView::Signals<1>& mySignals) {
    auto stateQuery = std::get<0>(mySignals).test_and_set();
    if (!stateQuery)
        std::get<0>(mySignals).clear();
    return stateQuery;
}

class BlinkLoop : public SOS::Behavior::EventLoop<SignalsImpl> {
    public:
    BlinkLoop(SOS::Behavior::EventLoop<SignalsImpl>::SignalType& signalbus) : SOS::Behavior::EventLoop<SignalsImpl>(signalbus) {
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
            std::get<SignalsImpl::blink>(this->_intrinsic).test_and_set();
            //run
            operator()();
            //blink off
            std::get<SignalsImpl::blink>(this->_intrinsic).clear();
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
    auto mySignals = SignalsImpl{};
    std::cout<<"Wire initialised to "<<get(mySignals)<<std::endl;
    std::cout<<"Wire before EventLoop start "<<get(mySignals)<<std::endl;
    BlinkLoop* myHandler = new BlinkLoop(mySignals);
    std::cout<<"Wire after EventLoop start "<<get(mySignals)<<std::endl;
    const auto start = high_resolution_clock::now();
    while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<3){
        //read as fast as possible to test atomic
        get(mySignals);
        /*if(get(mySignals)==true)
            std::cout<<"*";
        else
            std::cout<<"_";*/
    }
    //std::cout<<std::endl;
    std::cout<<"Wire before EventLoop teardown "<<get(mySignals)<<std::endl;
    delete myHandler;
    std::cout<<"Wire after EventLoop teardown "<<get(mySignals)<<std::endl;
}