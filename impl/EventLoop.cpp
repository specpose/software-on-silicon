#include "software-on-silicon/error.hpp"
#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/loop_helpers.hpp"
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

class BlinkLoop : public SOS::Behavior::Controller<SOS::MemoryView::Notify> {
    public:
    BlinkLoop(SOS::MemoryView::Notify& bussignal) :
    SOS::Behavior::Controller<SOS::MemoryView::Notify>(bussignal) {
        _thread=start(this);
    }
    ~BlinkLoop() final {
        _thread.join();
        std::cout<<"Thread has ended normally."<<std::endl;
    }
    void event_loop(){
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<10){
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