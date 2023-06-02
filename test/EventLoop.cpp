#include "software-on-silicon/EventLoop.hpp"
#include <iostream>

class MyFunctor {
    public:
    void operator()(){
        std::cout << message <<std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds{800});
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

class EventLoopImpl {
};

//Last member of hierarchy
template<> class SOS::Behavior::EventLoop<EventLoopImpl> {
    public:
    using SignalType = SignalsImpl;
    EventLoop(SignalType& signalbus) : _intrinsic(signalbus) {
        auto thread = std::thread{std::mem_fn(&EventLoop<EventLoopImpl>::event_loop),this};
        _thread.swap(thread);
    }
    ~EventLoop(){
        _thread.join();
    }
    void event_loop(){
        const auto start = std::chrono::high_resolution_clock::now();
        while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<10){
            //acquire new data through a wire
            //blink on
            std::get<SignalsImpl::blink>(this->_intrinsic).test_and_set();
            //run
            operator()();
            //blink off
            std::get<SignalsImpl::blink>(this->_intrinsic).clear();
            //pause
            std::this_thread::sleep_for(std::chrono::milliseconds{1200});
        }
    }
    void operator()(){
        predicate();
    }
    private:
    SignalType& _intrinsic;
    std::thread _thread = std::thread{};
    MyFunctor predicate = MyFunctor();
};

//template<> class EventHandler<EventLoopImpl> {};

int main () {
    auto mySignals = SignalsImpl{std::atomic_flag{}};
    auto stateQuery = std::get<SignalsImpl::blink>(mySignals).test_and_set();
    if (!stateQuery)
        std::get<SignalsImpl::blink>(mySignals).clear();
    std::cout<<"Wire initialised to "<<stateQuery<<std::endl;
    stateQuery = std::get<SignalsImpl::blink>(mySignals).test_and_set();
    if (!stateQuery)
        std::get<SignalsImpl::blink>(mySignals).clear();
    std::cout<<"Wire before EventLoop start "<<stateQuery<<std::endl;
    auto myHandler = SOS::Behavior::EventLoop<EventLoopImpl>(mySignals);
    stateQuery = std::get<SignalsImpl::blink>(mySignals).test_and_set();
    if (!stateQuery)
        std::get<SignalsImpl::blink>(mySignals).clear();
    std::cout<<"Wire after EventLoop start "<<stateQuery<<std::endl;
    const auto start = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<3){
        //read as fast as possible to test atomic
        auto stateQuery2 = std::get<SignalsImpl::blink>(mySignals).test_and_set();
        if (!stateQuery2)
            std::get<SignalsImpl::blink>(mySignals).clear();
        if(stateQuery2==true)
            std::cout<<"processing... ";
        else
            std::cout<<"halted... ";
    }
    std::cout<<std::endl;
    /*myHandler.join();
    stateQuery = std::get<SignalsImpl::blink>(mySignals).test_and_set();
    if (!stateQuery)
        std::get<SignalsImpl::blink>(mySignals).clear();
    std::cout<<"Wire after EventLoop end "<<stateQuery<<std::endl;*/
}