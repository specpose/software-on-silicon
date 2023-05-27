#include "software-on-silicon/TypedWires.hpp"
#include "stackable-functor-allocation/Thread.hpp"

#include <iostream>
#include <chrono>

enum {
    blink,
    quit
};

template<typename T>class HandlerImpl : public SOS::Behavior::EventHandler<T> {
    public:
    HandlerImpl(SOS::MemoryView::TypedWires<T>& databus) : SOS::Behavior::EventHandler<T>(databus) {}
    void operator()() final {
        while(true){
            std::get<blink>(this->_wires).store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds{800});
            std::get<blink>(this->_wires).store(false);
            std::this_thread::sleep_for(std::chrono::milliseconds{1200});
        }
    };
};

int main () {
    auto myWires = SOS::MemoryView::TypedWires<bool>{false};
    std::cout<<"Wire initialised to "<<std::get<0>(myWires).load()<<std::endl;
    auto myHandler = HandlerImpl<bool>(myWires);
    std::cout<<"Wire before EventHandler start "<<std::get<blink>(myWires).load()<<std::endl;
    auto myThread = SFA::Thread<HandlerImpl<bool>>(myHandler);
    std::cout<<"Wire after EventHandler start "<<std::get<blink>(myWires).load()<<std::endl;
    const auto start = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<3){
        if(std::get<blink>(myWires).load()==true)
            std::cout<<"true... ";
        else
            std::cout<<"false... ";
    }
    myThread();
    std::cout<<"Wire after EventHandler end "<<std::get<blink>(myWires).load()<<std::endl;
};