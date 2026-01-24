#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/simulation_helpers.hpp"

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

class BlinkLoop : public SOS::Behavior::DummySimpleController<> {
    public:
    BlinkLoop(SOS::MemoryView::Notify& bussignal) :
    SOS::Behavior::DummySimpleController<>(bussignal)
    ,start_tp(high_resolution_clock::now())
    {
        _thread=start(this);
    }
    ~BlinkLoop() final {
        destroy(_thread);
        std::cout<<"Thread has ended normally."<<std::endl;
    }
    void event_loop(){
            //would: acquire new data through a wire
            //blink on
            _intrinsic.getNotifyRef().clear();
            //run
            operator()();
            //blink off
            _intrinsic.getNotifyRef().test_and_set();
            //pause
            std::this_thread::sleep_for(milliseconds{666});
    }
    void operator()(){
        predicate();
    }
    private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_tp;
    MyFunctor predicate = MyFunctor();

    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};
