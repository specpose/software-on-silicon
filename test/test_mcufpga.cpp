#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#define DMA std::array<unsigned char,999>//1001%3=2
DMA mcu_to_fpga_buffer;
DMA fpga_to_mcu_buffer;
#include "software-on-silicon/Serial.hpp"
#include "software-on-silicon/MCUFPGA.hpp"
#include <limits>

using namespace SOS::MemoryView;

class FPGA : public SOS::Behavior::Loop, public SOS::Behavior::SerialFPGAController<DMA,DMA> {
    public:
    FPGA(bus_type& myBus) :
    Loop(),
    SOS::Behavior::SerialFPGAController<DMA,DMA>(myBus)
    {
        int writeBlinkCounter = 0;
        bool writeBlink = true;
        for (std::size_t i=0;i<std::get<0>(objects).size();i++){
            DMA::value_type data;
            if (writeBlink)
                data = 42;//'*'
            else
                data = 95;//'_'
            std::get<0>(objects)[i]=data;
            writeBlinkCounter++;
            if (writeBlink && writeBlinkCounter==333){
                writeBlink = false;
                writeBlinkCounter=0;
            } else if (!writeBlink && writeBlinkCounter==666) {
                writeBlink = true;
                writeBlinkCounter=0;
            }
        }
        descriptors[0].synced=false;
        int dontcount=0;
        write_hook(dontcount);//INIT: First byte of com-buffer needs to be valid
        _thread=start(this);
    }
    ~FPGA() {
        //_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        _thread.join();
        std::cout<<"FPGA Object 1"<<std::endl;
        for (std::size_t i=0;i<std::get<1>(objects).size();i++){
            printf("%c",std::get<1>(objects)[i]);
        }
        std::cout<<std::endl;
    }
    virtual void event_loop() final {
        int read4minus1 = 0;
        int write3plus1 = 0;
        while(stop_token.getUpdatedRef().test_and_set()){
            if (handshake()) {
            read_hook(read4minus1);
            if (!stateOfObjectOne&&descriptors[1].readLock)
                std::cout<<"Object1 read lock turned on"<<std::endl;
            else if (stateOfObjectOne&&!descriptors[1].readLock)
                std::cout<<"Object1 read lock turned off"<<std::endl;
            stateOfObjectOne = descriptors[1].readLock;
            write_hook(write3plus1);
            }
            std::this_thread::yield();
        }
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    bool stateOfObjectOne = false;
    std::thread _thread = std::thread{};
};
class MCUThread : public SOS::Behavior::Loop, public SOS::Behavior::SerialMCUThread<FPGA,DMA,DMA> {
    public:
    MCUThread() :
    Loop(),
    SOS::Behavior::SerialMCUThread<FPGA,DMA,DMA>() {
        std::get<1>(objects).fill('-');
        descriptors[1].synced=false;
        _thread=start(this);
    }
    ~MCUThread() {
        Thread<FPGA>::_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
        std::cout<<"MCU Object 0"<<std::endl;
        for (std::size_t i=0;i<std::get<0>(objects).size();i++){
            printf("%c",std::get<0>(objects)[i]);
        }
        std::cout<<std::endl;
    }
    void event_loop(){
        int read4minus1 = 0;
        int write3plus1 = 0;
        while(stop_token.getUpdatedRef().test_and_set()){
            if (handshake()) {
            read_hook(read4minus1);
            if (!stateOfObjectZero&&descriptors[0].readLock)
                std::cout<<"Object0 read lock turned on"<<std::endl;
            else if (stateOfObjectZero&&!descriptors[0].readLock)
                std::cout<<"Object0 read lock turned off"<<std::endl;
            stateOfObjectZero = descriptors[0].readLock;
            write_hook(write3plus1);
            }
            std::this_thread::yield();
        }
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    bool stateOfObjectZero = false;
    std::thread _thread = std::thread{};
};

int main () {
    auto host= MCUThread();
    const auto start = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < 1) {
        std::this_thread::yield();
    }
    //host._child.stop();
    host.stop();
}
