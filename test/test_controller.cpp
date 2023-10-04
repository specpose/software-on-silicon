#include "Controller.cpp"

int main () {
    std::cout<<"Controller loop running for 5s..."<<std::endl;
    ControllerImpl* myController = new ControllerImpl();
    const auto start = high_resolution_clock::now();
    while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<10){//REMOVE: Needs signaling
        while(duration_cast<seconds>(high_resolution_clock::now()-start).count()<5){//REMOVE: Needs signaling
            std::this_thread::yield();
        }
        myController->stop();
        std::this_thread::yield();
    }
    delete myController;
}