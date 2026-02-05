#define measurement_unit_in_ms 100
#include "Controller.cpp"

int main () {
    SOS::MemoryView::BusAsyncAndShaker bus{};
    std::cout<<"Controller loop running for 5s..."<<std::endl;
    ControllerImpl* myController = new ControllerImpl(bus);
    const auto start = high_resolution_clock::now();
    std::cout<<"main() waiting for 5s..."<<std::endl;
    std::this_thread::sleep_for(seconds{5});
    bus.signal.getAuxUpdatedRef().clear();
    delete myController;
}