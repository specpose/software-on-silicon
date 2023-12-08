#include <iostream>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/Serial.hpp"
#define DMA std::array<unsigned char,999>//1001%3=2
DMA mcu_to_fpga_buffer;
DMA fpga_to_mcu_buffer;
#include "software-on-silicon/MCUFPGA.hpp"
#include "software-on-silicon/mcufpga_helpers.hpp"
#include <limits>

using namespace SOS::MemoryView;
class CounterTask {
    public:
    //using reader_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<ReadBufferType>::const_cables_type>::type;
};
class FPGAProcessingSwitch {
    public:
    using process_notifier_ct = typename std::tuple_element<0,typename SOS::MemoryView::SerialProcessNotifier::const_cables_type>::type;
    FPGAProcessingSwitch(process_notifier_ct& notifier) : _notifier(notifier) {}
    void read_notify_hook(){
        auto object_id = _notifier.getReadDestinationRef().load();
        switch(object_id){
            case 0:
            std::cout<<"FPGA received: "<<object_id<<std::endl;
            break;
            case 1:
            std::cout<<"FPGA received: "<<object_id<<std::endl;
            break;
            case 2:
            std::cout<<"FPGA received: "<<object_id<<std::endl;
            break;
        }
    }
    void write_notify_hook(){
        auto object_id = _notifier.getWriteOriginRef().load();
        switch(object_id){
            case 0:
            std::cout<<"FPGA written: "<<object_id<<std::endl;
            break;
            case 1:
            std::cout<<"FPGA written: "<<object_id<<std::endl;
            break;
            case 2:
            std::cout<<"FPGA written: "<<object_id<<std::endl;
            break;
        }
    }
    private:
    process_notifier_ct& _notifier;
};
class MCUProcessingSwitch {
    public:
    using process_notifier_ct = typename std::tuple_element<0,typename SOS::MemoryView::SerialProcessNotifier::const_cables_type>::type;
    MCUProcessingSwitch(process_notifier_ct& notifier) : _notifier(notifier) {}
    void read_notify_hook(){
        auto object_id = _notifier.getReadDestinationRef().load();
        switch(object_id){
            case 0:
            std::cout<<"MCU received: "<<object_id<<std::endl;
            break;
            case 1:
            std::cout<<"MCU received: "<<object_id<<std::endl;
            break;
            case 2:
            std::cout<<"MCU received: "<<object_id<<std::endl;
            break;
        }
    }
    void write_notify_hook(){
        auto object_id = _notifier.getWriteOriginRef().load();
        switch(object_id){
            case 0:
            std::cout<<"MCU written: "<<object_id<<std::endl;
            break;
            case 1:
            std::cout<<"MCU written: "<<object_id<<std::endl;
            break;
            case 2:
            std::cout<<"MCU written: "<<object_id<<std::endl;
            break;
        }
    }
    private:
    process_notifier_ct& _notifier;
};
template<typename ProcessingSwitch> class SerialProcessingImpl : public SOS::Behavior::SerialProcessing<ProcessingSwitch> {
    public:
    SerialProcessingImpl(typename SOS::Behavior::SerialProcessing<ProcessingSwitch>::bus_type& bus) : SOS::Behavior::SerialProcessing<ProcessingSwitch>(bus) {
        _thread=SOS::Behavior::Loop::start(this);
    }
    ~SerialProcessingImpl(){
        SOS::Behavior::SerialProcessing<ProcessingSwitch>::stop_token.getUpdatedRef().clear();
        _thread.join();
    }
    private:
    std::thread _thread = std::thread{};
};
class FPGA : private virtual SOS::Protocol::SerialFPGA<SerialProcessingImpl<FPGAProcessingSwitch>, DMA,DMA>,
public SOS::Behavior::SimulationFPGA<SerialProcessingImpl<FPGAProcessingSwitch>,DMA,DMA> {
    public:
    using bus_type = SOS::MemoryView::BusShaker;
    FPGA(bus_type& myBus) :
    SOS::Behavior::EventController<SerialProcessingImpl<FPGAProcessingSwitch>>(myBus.signal),
    SOS::Protocol::SerialFPGA<SerialProcessingImpl<FPGAProcessingSwitch>, DMA, DMA>(),
    SOS::Behavior::SimulationFPGA<SerialProcessingImpl<FPGAProcessingSwitch>,DMA,DMA>(mcu_to_fpga_buffer,fpga_to_mcu_buffer)
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
        boot_time = std::chrono::high_resolution_clock::now();
        _thread=start(this);
    }
    ~FPGA() {
        _child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout<<"Dumping FPGA DMA Objects"<<std::endl;
        dump_objects(objects,descriptors,boot_time,kill_time);
    }
    private:
    bool stateOfObjectOne = false;
    bool syncStateObjectOne = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};
class MCUThread : private virtual SOS::Protocol::SerialMCU<SerialProcessingImpl<MCUProcessingSwitch>, DMA,DMA>,
public SOS::Behavior::SimulationMCU<SerialProcessingImpl<MCUProcessingSwitch>,DMA,DMA> {
    public:
    using bus_type = SOS::MemoryView::BusShaker;
    MCUThread(bus_type& myBus) :
    SOS::Behavior::EventController<SerialProcessingImpl<MCUProcessingSwitch>>(myBus.signal),
    SOS::Protocol::SerialMCU<SerialProcessingImpl<MCUProcessingSwitch>, DMA, DMA>(),
    SOS::Behavior::SimulationMCU<SerialProcessingImpl<MCUProcessingSwitch>,DMA,DMA>(fpga_to_mcu_buffer,mcu_to_fpga_buffer) {
        std::get<1>(objects).fill('-');
        descriptors[1].synced=false;
        boot_time = std::chrono::high_resolution_clock::now();
        _thread=start(this);
    }
    ~MCUThread() {
        //Thread<FPGA>::_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout<<"Dumping MCU DMA Objects"<<std::endl;
        dump_objects(objects,descriptors,boot_time,kill_time);
    }
    private:
    bool stateOfObjectZero = false;
    bool syncStateObjectZero = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};