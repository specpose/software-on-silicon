#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/simulation_helpers.hpp"

using namespace SOS::MemoryView;
using namespace std::chrono;

class NotifyImpl : public SOS::Behavior::SimpleDummy {
public:
    using bus_type = SOS::MemoryView::BusNotifier;
    NotifyImpl(bus_type& bus)
        : SOS::Behavior::SimpleDummy(bus.signal)
    {
        _thread = start(this);
    }
    ~NotifyImpl() final
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

class ProcessorScheme : public SOS::Behavior::SimpleDummy {
public:
    using bus_type = SOS::MemoryView::BusNotifier;
    ProcessorScheme(bus_type& bus)
    : SOS::Behavior::SimpleDummy(bus.signal)
    {
        _intrinsic.getNotifyRef().clear();
        _thread = start(this);
    }
    ~ProcessorScheme() final
    {
        destroy(_thread);
    }
    void event_loop()
    {
        for (int i = 0; i < 3; ++i) {
        // would: acquire new data through a wire
        // blink on
        while (_intrinsic.getNotifyRef().test_and_set())
            std::this_thread::yield();
        // run
        std::cout << "*";
        // blink off
        _intrinsic.getNotifyRef().clear();
        std::this_thread::sleep_for(milliseconds { 111 });
        }
        // pause
        for (int i = 0; i < 6; ++i) {
        std::this_thread::sleep_for(milliseconds { 111 });
        }
    };

private:
    std::thread _thread;
};

template <typename S>
class ControllerImpl : public SOS::Behavior::EventController<S> {
public:
    ControllerImpl(typename SOS::Behavior::EventController<S>::bus_type& bus)
    : SOS::Behavior::EventController<S>(bus.signal)
    , waiter(new SystemTimer<milliseconds, MEASUREMENT_UNIT_IN_MILLIS>(waiterBus.signal))
    {
        _thread = this->start(this);
    }
    ~ControllerImpl()
    {
        this->destroy(_thread);
        std::cout << "Controller loop has terminated." << std::endl;
        delete waiter;
    }
    void event_loop();

private:
    bool stop = false;
    SOS::MemoryView::BusShaker waiterBus {};
    SystemTimer<milliseconds, MEASUREMENT_UNIT_IN_MILLIS>* waiter;
    std::thread _thread;
};

template <> void ControllerImpl<NotifyImpl>::event_loop()
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

template <> void ControllerImpl<ProcessorScheme>::event_loop()
{
    if (!_intrinsic.getUpdatedRef().test_and_set()) {
        waiterBus.signal.getUpdatedRef().clear();
        if (!waiterBus.signal.getAcknowledgeRef().test_and_set()) {
            for (int i = 0; i < 3; ++i) {
            while (_foreign.signal.getNotifyRef().test_and_set())
                std::this_thread::yield();
            std::cout << "_";
            _foreign.signal.getNotifyRef().clear();
            std::this_thread::sleep_for(milliseconds { 111 });
            }
        }
        _intrinsic.getAcknowledgeRef().clear();
    }
}
