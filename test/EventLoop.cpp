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

static bool get(SOS::MemoryView::Signals<1>& mySignals,size_t signal) {
    auto stateQuery = std::get<SignalsImpl::blink>(mySignals).test_and_set();
    if (!stateQuery)
        std::get<SignalsImpl::blink>(mySignals).clear();
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
    std::thread _thread = std::thread{};
    MyFunctor predicate = MyFunctor();
};

int main () {
    auto mySignals = SignalsImpl{std::atomic_flag{}};
    std::cout<<"Wire initialised to "<<get(mySignals,SignalsImpl::blink)<<std::endl;
    std::cout<<"Wire before EventLoop start "<<get(mySignals,SignalsImpl::blink)<<std::endl;
    BlinkLoop* myHandler = new BlinkLoop(mySignals);
    std::cout<<"Wire after EventLoop start "<<get(mySignals,SignalsImpl::blink)<<std::endl;
    const auto start = high_resolution_clock::now();
    while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<3){
        //read as fast as possible to test atomic
        get(mySignals,SignalsImpl::blink);
        /*if(get(mySignals,SignalsImpl::blink)==true)
            std::cout<<"*";
        else
            std::cout<<"_";*/
    }
    //std::cout<<std::endl;
    std::cout<<"Wire before EventLoop teardown "<<get(mySignals,SignalsImpl::blink)<<std::endl;
    delete myHandler;
    std::cout<<"Wire after EventLoop teardown "<<get(mySignals,SignalsImpl::blink)<<std::endl;
}