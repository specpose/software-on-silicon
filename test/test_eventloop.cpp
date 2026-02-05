#include "EventLoop.cpp"

#define measurement_unit_in_ms 100

int main (int argc, char *argv[]) {
    auto waiterBus = SOS::MemoryView::BusShaker{};
    TimerIF<milliseconds,measurement_unit_in_ms>* waiter = nullptr;
    unsigned long ticks = 0;
    if (argc == 2 && sscanf(argv[1], "%i", &ticks)==1)
        waiter = new TickTimer<milliseconds,measurement_unit_in_ms>(waiterBus.signal,ticks);
    else
        waiter = new SystemTimer<milliseconds,measurement_unit_in_ms>(waiterBus.signal);

    auto myBus = BusNotifier{};
    std::cout<<"Thread running for 10s..."<<std::endl;
    BlinkLoop* myHandler = new BlinkLoop(myBus.signal);
    const auto start = high_resolution_clock::now();
    std::cout<<"main() loop running for 5s..."<<std::endl;
    while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<5){
        waiterBus.signal.getUpdatedRef().clear();
        if (!waiterBus.signal.getAcknowledgeRef().test_and_set()){
            if (!myBus.signal.getNotifyRef().test_and_set()) {
                myBus.signal.getNotifyRef().clear();
                std::cout<<"*";
            } else {
                std::cout<<"_";
            }
        }
        std::this_thread::yield();
    }
    std::cout<<std::endl<<"main() loop has terminated."<<std::endl;
    std::this_thread::sleep_for(seconds{5});
    delete myHandler;
    delete waiter;
}
