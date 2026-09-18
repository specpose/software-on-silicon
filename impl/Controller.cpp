#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/simulation_helpers.hpp"

using namespace SOS::MemoryView;
using namespace std::chrono;

// needs a bus because it is a subcontroller
class SubControllerImpl : public SOS::Behavior::SimpleDummy {
public:
    using bus_type = SOS::MemoryView::BusNotifier;
    SubControllerImpl(bus_type& bus)
        : SOS::Behavior::SimpleDummy(bus.signal)
    {
        _thread = start(this);
    }
    ~SubControllerImpl() final
    {
        destroy(_thread);
    }
    void event_loop()
    {
        // would: acquire new data through a wire
        // blink on
        _intrinsic.getNotifyRef().clear();
        // run
        std::this_thread::sleep_for(milliseconds { 333 });
        // blink off
        _intrinsic.getNotifyRef().test_and_set();
        // pause
        std::this_thread::sleep_for(milliseconds { 666 });
    };

private:
    std::thread _thread;
};

// A RunLoop is not a Loop, because it does not have a signal
class ControllerImpl : public SOS::Behavior::EventController<SubControllerImpl> {
public:
    ControllerImpl(bus_type& bus)
        : EventController<SubControllerImpl>(bus.signal)
        , waiter(new SystemTimer<milliseconds, MEASUREMENT_UNIT_IN_MILLIS>(waiterBus.signal))
    {
        _thread = start(this);
    }
    ~ControllerImpl()
    {
        destroy(_thread);
        std::cout << "Controller loop has terminated." << std::endl;
        delete waiter;
    }
    void event_loop()
    {
        if (!_intrinsic.getUpdatedRef().test_and_set()) {
            waiterBus.signal.getUpdatedRef().clear();
            if (!waiterBus.signal.getAcknowledgeRef().test_and_set()) {
                if (!_foreign.signal.getNotifyRef().test_and_set()) {
                    _foreign.signal.getNotifyRef().clear();
                    std::cout << "*";
                } else {
                    std::cout << "_";
                }
            }
            _intrinsic.getAcknowledgeRef().clear();
        }
    }

private:
    bool stop = false;
    SOS::MemoryView::BusShaker waiterBus {};
    SystemTimer<milliseconds, MEASUREMENT_UNIT_IN_MILLIS>* waiter;
    std::thread _thread;
};
