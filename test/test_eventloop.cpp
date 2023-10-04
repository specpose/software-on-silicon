#include "EventLoop.cpp"

int main () {
    auto waiterBus = SOS::MemoryView::BusShaker{};
    auto waiter = Timer<milliseconds,100>(waiterBus.signal);

    auto myBus = BusNotifier{};
    std::cout<<"Thread running for 10s..."<<std::endl;
    BlinkLoop* myHandler = new BlinkLoop(myBus.signal);
    std::cout<<"main() loop running for 5s..."<<std::endl;
    const auto start = high_resolution_clock::now();
    while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<10){
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
    }
    myHandler->stop();
    delete myHandler;
}