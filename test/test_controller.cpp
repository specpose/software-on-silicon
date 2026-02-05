#include "Controller.cpp"

int main () {
    std::cout<<"Controller loop running for 5s..."<<std::endl;
    SOS::MemoryView::BusAsyncAndShaker bus{};
    ControllerImpl* myController = new ControllerImpl(bus);
    const auto start = high_resolution_clock::now();
    std::this_thread::sleep_for(seconds{5});
    bus.signal.getAuxUpdatedRef().clear();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() <5)
        std::this_thread::yield();
    delete myController;
}