#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/loop_helpers.hpp"
#include <iostream>

using namespace SOS::MemoryView;
using namespace std::chrono;

//needs a bus because it is a subcontroller
class SubControllerImpl : public SOS::Behavior::SimpleController<SOS::Behavior::DummyController> {
    public:
    SubControllerImpl(BusNotifier& bus) :
    SOS::Behavior::SimpleController<SOS::Behavior::DummyController>(bus.signal) {
        std::cout<<"SubController running for 10s..."<<std::endl;
        _thread=start(this);
    }
    ~SubControllerImpl() final {
        _thread.join();
        std::cout<<"SubController has ended normally."<<std::endl;
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
    };
    void operator()(){
        std::this_thread::sleep_for(milliseconds{_duration});
    }
    private:
    unsigned int _duration = 333;

    std::thread _thread = std::thread{};
};

//A RunLoop is not a Loop, because it does not have a signal
class ControllerImpl : public Thread<SubControllerImpl> {
    public:
    ControllerImpl() : Thread<SubControllerImpl>() {
        _thread=start(this);
    }
    ~ControllerImpl() final {
        _thread.join();
    }
    void event_loop(){
        auto waiterBus = SOS::MemoryView::BusShaker{};
        auto waiter = Timer<milliseconds,100>(waiterBus.signal);

        std::cout<<"Controller loop running for 5s..."<<std::endl;
        const auto start = high_resolution_clock::now();
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<5){
            waiterBus.signal.getUpdatedRef().clear();
            if (!waiterBus.signal.getAcknowledgeRef().test_and_set()){
                operator()();
            }
            std::this_thread::yield();
        }
        std::cout<<std::endl<<"Controller loop has terminated."<<std::endl;
    }
    void operator()(){
        if (!_foreign.signal.getNotifyRef().test_and_set()) {
            _foreign.signal.getNotifyRef().clear();
            printf("*");
        } else {
            printf("_");
        }
    }
    private:
    std::thread _thread = std::thread{};
};

int main () {
    ControllerImpl* myController = new ControllerImpl();
    delete myController;
}