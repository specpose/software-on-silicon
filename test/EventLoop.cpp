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

int main () {
    auto waiterBus = SOS::MemoryView::BusShaker{};
    auto waiter = Timer<milliseconds,100>(waiterBus.signal);

    auto myBus = BusNotifier{};
    std::cout<<"Thread running for 10s..."<<std::endl;
    BlinkLoop* myHandler = new BlinkLoop(myBus.signal);
    std::cout<<"main() loop running for 5s..."<<std::endl;
    const auto start = high_resolution_clock::now();
    while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<5){
        waiterBus.signal.getUpdatedRef().clear();
        if (!waiterBus.signal.getAcknowledgeRef().test_and_set()){
            if (!myBus.signal.getNotifyRef().test_and_set()) {
                myBus.signal.getNotifyRef().clear();
                printf("*");
            } else {
                printf("_");
            }
        }
        std::this_thread::yield();
    }
    std::cout<<std::endl<<"main() loop has terminated."<<std::endl;
    delete myHandler;
}