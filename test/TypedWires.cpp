#include "software-on-silicon/TypedWires.hpp"
#include "stackable-functor-allocation/Thread.hpp"

#include <iostream>
#include <chrono>

class MyFunctor {
    public:
    void operator()(){
        std::cout << message <<std::endl;
    }
    private:
    std::string message = std::string{"Thread is running."};
};

enum {
    blink
};

class HandlerImpl : public SOS::Behavior::EventHandler<bool> {
    public:
    HandlerImpl(WireType& databus) : SOS::Behavior::EventHandler<bool>(databus) {}
    void operator()(){
        predicate();
    }
    void eventloop() {
        this->predicate();
        const auto start = std::chrono::high_resolution_clock::now();
        while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<10){
            std::get<blink>(this->_intrinsic).store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds{800});
            std::get<blink>(this->_intrinsic).store(false);
            std::this_thread::sleep_for(std::chrono::milliseconds{1200});
        }
    };
    private:
    MyFunctor predicate = MyFunctor();
};

int main () {
    auto myWires = HandlerImpl::WireType{false};
    std::cout<<"Wire initialised to "<<std::get<0>(myWires).load()<<std::endl;
    std::cout<<"Wire before EventHandler start "<<std::get<blink>(myWires).load()<<std::endl;
    auto myHandler = HandlerImpl(myWires);
    std::cout<<"Wire after EventHandler start "<<std::get<blink>(myWires).load()<<std::endl;
    const auto start = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<3){
        if(std::get<blink>(myWires).load()==true)
            std::cout<<"true... ";
        else
            std::cout<<"false... ";
    }
    std::cout<<std::endl;
    myHandler.join();
    std::cout<<"Wire after EventHandler end "<<std::get<blink>(myWires).load()<<std::endl;
};