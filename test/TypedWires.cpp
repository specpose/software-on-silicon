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

enum {
    blink
};

class HandlerImpl : public SOS::Behavior::EventLoop<std::atomic<bool>> {
    public:
    using WireType = SOS::MemoryView::TypedWires<std::atomic<bool>>;
    HandlerImpl(WireType& databus) : SOS::Behavior::EventLoop<std::atomic<bool>>(databus) {}
    void operator()(){
        predicate();
    }
    void eventloop() {
        const auto start = std::chrono::high_resolution_clock::now();
        while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<10){
            //acquire new data through a wire
            //blink on
            std::get<blink>(this->_intrinsic).store(true);
            //run
            operator()();
            //blink off
            std::get<blink>(this->_intrinsic).store(false);
            //pause
            std::this_thread::sleep_for(std::chrono::milliseconds{1200});
        }
    };
    private:
    MyFunctor predicate = MyFunctor();
};

int main () {
    auto myWires = HandlerImpl::WireType{false};
    std::cout<<"Wire initialised to "<<std::get<0>(myWires).load()<<std::endl;
    std::cout<<"Wire before EventLoop start "<<std::get<blink>(myWires).load()<<std::endl;
    auto myHandler = HandlerImpl(myWires);
    std::cout<<"Wire after EventLoop start "<<std::get<blink>(myWires).load()<<std::endl;
    const auto start = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<3){
        //read as fast as possible to test atomic
        if(std::get<blink>(myWires).load()==true)
            std::cout<<"processing... ";
        else
            std::cout<<"halted... ";
    }
    std::cout<<std::endl;
    myHandler.join();
    std::cout<<"Wire after EventLoop end "<<std::get<blink>(myWires).load()<<std::endl;
};