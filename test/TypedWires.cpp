#include "software-on-silicon/TypedWires.hpp"
#include "stackable-functor-allocation/Thread.hpp"

#include <iostream>
#include <chrono>

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

class HandlerImpl : public SOS::Behavior::EventLoop<1> {
    public:
    //using WireType = SOS::MemoryView::TypedWires<std::atomic<bool>>;
    HandlerImpl(SignalsImpl& databus) : SOS::Behavior::EventLoop<1>(databus) {}
    void operator()(){
        predicate();
    }
    void eventloop() {
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
    };
    private:
    MyFunctor predicate = MyFunctor();
};

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
    auto myHandler = HandlerImpl(mySignals);
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
    myHandler.join();
    stateQuery = std::get<SignalsImpl::blink>(mySignals).test_and_set();
    if (!stateQuery)
        std::get<SignalsImpl::blink>(mySignals).clear();
    std::cout<<"Wire after EventLoop end "<<stateQuery<<std::endl;
};