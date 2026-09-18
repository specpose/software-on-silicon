#include "Controller.cpp"

int main()
{
    SOS::MemoryView::BusShaker bus {};
    std::cout << "Controller loop running for 10s..." << std::endl;
    ControllerImpl* myController = new ControllerImpl(bus);
    const auto start = high_resolution_clock::now();
    std::cout << "main() loop running for 5s..." << std::endl;
    bus.signal.getUpdatedRef().clear();
    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < 5) {
        if (!bus.signal.getAcknowledgeRef().test_and_set())
            bus.signal.getUpdatedRef().clear();
    }
    std::cout << std::endl << "main() loop has terminated." << std::endl;
    std::this_thread::sleep_for(seconds { 5 });
    delete myController;
}