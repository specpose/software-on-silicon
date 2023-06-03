#include "software-on-silicon/Controller.hpp"

class DummySubController : public SOS::Behavior::EventLoop<SOS::MemoryView::Signals<0>> {
    public:
    DummySubController(SOS::Behavior::EventLoop<SOS::MemoryView::Signals<0>>::SignalType& signalbus) : SOS::Behavior::EventLoop<SOS::MemoryView::Signals<0>>(signalbus) {
    }
    ~DummySubController(){
    }
    void event_loop(){
    };
};

class ControllerImpl : public SOS::Behavior::Controller<
SOS::MemoryView::Signals<0>,
DummySubController
> {
    public:
    using SOS::Behavior::Controller<
    SOS::MemoryView::Signals<0>,
    DummySubController
    >::Controller;
    void event_loop(){}
};

int main () {
    auto myWires = SOS::MemoryView::Signals<0>{};
    auto myController = ControllerImpl(myWires);
}