#include <iostream>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/serial_helpers.hpp"
#include "software-on-silicon/Serial.hpp"
#define DMA std::array<unsigned char,999>//1001%3=2
DMA mcu_to_fpga_buffer;
DMA fpga_to_mcu_buffer;
#include "software-on-silicon/MCUFPGA.hpp"
#include "software-on-silicon/mcufpga_helpers.hpp"
#include <limits>

using namespace SOS::MemoryView;

class FPGA : public SOS::Behavior::SerialFPGAController<DMA,DMA> {
    public:
    FPGA(bus_type& myBus) :
    SOS::Behavior::SerialFPGAController<DMA,DMA>(myBus,mcu_to_fpga_buffer,fpga_to_mcu_buffer)
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
        boot_time = high_resolution_clock::now();
        _thread=start(this);
    }
    ~FPGA() {
        //_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
        kill_time = high_resolution_clock::now();
        std::cout<<"Dumping FPGA DMA Objects"<<std::endl;
        dump_objects(objects,descriptors,boot_time,kill_time);
    }
    private:
    virtual void signaling_hook() final {
        
    }
    bool stateOfObjectOne = false;
    bool syncStateObjectOne = true;

    std::chrono::time_point<high_resolution_clock> boot_time;
    std::chrono::time_point<high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};
class MCUThread : public SOS::Behavior::SerialMCUThread<DMA,DMA> {
    public:
    MCUThread(bus_type& myBus) :
    SOS::Behavior::SerialMCUThread<DMA,DMA>(myBus,fpga_to_mcu_buffer,mcu_to_fpga_buffer) {
        std::get<1>(objects).fill('-');
        descriptors[1].synced=false;
        boot_time = high_resolution_clock::now();
        _thread=start(this);
    }
    ~MCUThread() {
        //Thread<FPGA>::_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
        kill_time = high_resolution_clock::now();
        std::cout<<"Dumping MCU DMA Objects"<<std::endl;
        dump_objects(objects,descriptors,boot_time,kill_time);
    }
    private:
    virtual void signaling_hook(){
        
    }
    bool stateOfObjectZero = false;
    bool syncStateObjectZero = true;

    std::chrono::time_point<high_resolution_clock> boot_time;
    std::chrono::time_point<high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};