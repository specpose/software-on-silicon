#include <iostream>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/Serial.hpp"
#define DMA std::array<unsigned char,999>//1001%3=2
DMA mcu_to_fpga_buffer;
DMA fpga_to_mcu_buffer;
#include "software-on-silicon/MCUFPGA.hpp"
#include "software-on-silicon/mcufpga_helpers.hpp"
#include <limits>

using namespace SOS::MemoryView;
struct SymbolRateCounter {
    SymbolRateCounter(){
        setNumber(0);
        set_mcu_owned();
    }
    bool mcu_owned(){
        std::bitset<8> first_byte{static_cast<unsigned int>(StatusAndNumber[0])};
        first_byte = (first_byte >> 7) << 7;
        bool result = first_byte[7];//bigend => numbering is right to left, Stroustrup 34.2.2
        return result;
    }
    void set_mcu_owned(bool state=true){
        std::bitset<8> save_first_byte{static_cast<unsigned int>(StatusAndNumber[0])};
        save_first_byte[7]= state;
        StatusAndNumber[0]=static_cast<decltype(StatusAndNumber)::value_type>(save_first_byte.to_ulong());
    }
    unsigned int getNumber(){
        std::bitset<24> combined_bits;
        SOS::Protocol::bytearrayToBitset(combined_bits,StatusAndNumber);
        auto stripped = ((combined_bits << 1) >> 1);
        return stripped.to_ulong();
    }
    void setNumber(unsigned int number){
        bool save_ownership = mcu_owned();
        std::bitset<24> combined_bits{number};
        combined_bits[23]=save_ownership;//mcu_owned        
        SOS::Protocol::bitsetToBytearray(StatusAndNumber,combined_bits);
    }
    std::array<unsigned char,3> StatusAndNumber;//23 bits number: unsigned maxInt 8388607
};
class FPGAProcessingSwitch {
    public:
    using notifier_bt = typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA>;
    FPGAProcessingSwitch(notifier_bt& bus) : _nBus(bus) {}
    void read_notify_hook(){
        auto object_id = std::get<0>(_nBus.cables).getReadDestinationRef();
        switch(object_id){
            case 0:
            //std::cout<<"FPGA received: "<<object_id<<std::endl;
            if (!std::get<0>(_nBus.objects).mcu_owned()){
                auto n = std::get<0>(_nBus.objects).getNumber();
                std::get<0>(_nBus.objects).setNumber(++n);
                std::get<0>(_nBus.objects).set_mcu_owned(true);
                _nBus.descriptors[0].synced=false;
            }
            break;
            case 1:
            //std::cout<<"FPGA received: "<<object_id<<std::endl;
            break;
            case 2:
            //std::cout<<"FPGA received: "<<object_id<<std::endl;
            break;
        }
    }
    void write_notify_hook(){
        auto object_id = std::get<0>(_nBus.cables).getWriteOriginRef();
        switch(object_id){
            case 0:
            //std::cout<<"FPGA written: "<<object_id<<std::endl;
            break;
            case 1:
            //std::cout<<"FPGA written: "<<object_id<<std::endl;
            break;
            case 2:
            //std::cout<<"FPGA written: "<<object_id<<std::endl;
            break;
        }
    }
    private:
    notifier_bt& _nBus;
};
class MCUProcessingSwitch {
    public:
    using notifier_bt = typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA>;
    MCUProcessingSwitch(notifier_bt& bus) : _nBus(bus) {}
    void read_notify_hook(){
        auto object_id = std::get<0>(_nBus.cables).getReadDestinationRef();
        switch(object_id){
            case 0:
            //std::cout<<"MCU received: "<<object_id<<std::endl;
            if (std::get<0>(_nBus.objects).mcu_owned()){
                auto n = std::get<0>(_nBus.objects).getNumber();
                std::get<0>(_nBus.objects).setNumber(++n);
                std::get<0>(_nBus.objects).set_mcu_owned(false);
                _nBus.descriptors[0].synced=false;
            }
            break;
            case 1:
            //std::cout<<"MCU received: "<<object_id<<std::endl;
            break;
            case 2:
            //std::cout<<"MCU received: "<<object_id<<std::endl;
            break;
        }
    }
    void write_notify_hook(){
        auto object_id = std::get<0>(_nBus.cables).getWriteOriginRef();
        switch(object_id){
            case 0:
            //std::cout<<"MCU written: "<<object_id<<std::endl;
            break;
            case 1:
            //std::cout<<"MCU written: "<<object_id<<std::endl;
            break;
            case 2:
            //std::cout<<"MCU written: "<<object_id<<std::endl;
            break;
        }
    }
    private:
    notifier_bt& _nBus;
};
template<typename ProcessingSwitch> class SerialProcessingImpl : public SOS::Behavior::SerialProcessing<ProcessingSwitch, SymbolRateCounter, DMA, DMA> {
    public:
    SerialProcessingImpl(typename SOS::Behavior::SerialProcessing<ProcessingSwitch, SymbolRateCounter, DMA, DMA>::bus_type& bus) :
    SOS::Behavior::SerialProcessing<ProcessingSwitch, SymbolRateCounter, DMA, DMA>(bus) {
        _thread=SOS::Behavior::Loop::start(this);
    }
    ~SerialProcessingImpl(){
        SOS::Behavior::SerialProcessing<ProcessingSwitch, SymbolRateCounter, DMA, DMA>::stop_token.getUpdatedRef().clear();
        _thread.join();
    }
    private:
    std::thread _thread = std::thread{};
};
class FPGA : private virtual SOS::Protocol::SerialFPGA<SerialProcessingImpl<FPGAProcessingSwitch>>,
public SOS::Behavior::SimulationFPGA<SerialProcessingImpl<FPGAProcessingSwitch>> {
    public:
    using bus_type = SOS::MemoryView::BusShaker;
    FPGA(bus_type& myBus) :
    SOS::Behavior::PassthruEventController<SerialProcessingImpl<FPGAProcessingSwitch>>(myBus.signal,_dma),
    SOS::Protocol::SerialFPGA<SerialProcessingImpl<FPGAProcessingSwitch>>(),
    SOS::Behavior::SimulationFPGA<SerialProcessingImpl<FPGAProcessingSwitch>>(mcu_to_fpga_buffer,fpga_to_mcu_buffer)
    {
        _foreign.descriptors[0].synced=false;//COUNTER MCU owns it, so FPGA has to trigger a transfer
        int writeBlinkCounter = 0;
        bool writeBlink = true;
        for (std::size_t i=0;i<std::get<1>(_foreign.objects).size();i++){
            DMA::value_type data;
            if (writeBlink)
                data = 42;//'*'
            else
                data = 95;//'_'
            std::get<1>(_foreign.objects)[i]=data;
            writeBlinkCounter++;
            if (writeBlink && writeBlinkCounter==333){
                writeBlink = false;
                writeBlinkCounter=0;
            } else if (!writeBlink && writeBlinkCounter==666) {
                writeBlink = true;
                writeBlinkCounter=0;
            }
        }
        _foreign.descriptors[1].synced=false;
        boot_time = std::chrono::high_resolution_clock::now();
        _thread=start(this);
    }
    ~FPGA() {
        _child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout<<"FPGA counted "<<std::get<0>(_foreign.objects).getNumber()<<std::endl;
        std::cout<<"Dumping FPGA DMA Objects"<<std::endl;
        dump_objects(_foreign.objects,_foreign.descriptors,boot_time,kill_time);
    }
    private:
    typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA> _dma;
    bool stateOfObjectOne = false;
    bool syncStateObjectOne = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};
class MCUAsync : private virtual SOS::Protocol::SerialMCU<SerialProcessingImpl<MCUProcessingSwitch>>,
public SOS::Behavior::SimulationMCU<SerialProcessingImpl<MCUProcessingSwitch>> {
    public:
    using bus_type = SOS::MemoryView::BusShaker;
    MCUAsync(bus_type& myBus) :
    SOS::Behavior::PassthruEventController<SerialProcessingImpl<MCUProcessingSwitch>>(myBus.signal,_dma),
    SOS::Protocol::SerialMCU<SerialProcessingImpl<MCUProcessingSwitch>>(),
    SOS::Behavior::SimulationMCU<SerialProcessingImpl<MCUProcessingSwitch>>(fpga_to_mcu_buffer,mcu_to_fpga_buffer) {
        std::get<2>(_foreign.objects).fill('-');
        _foreign.descriptors[2].synced=false;
        boot_time = std::chrono::high_resolution_clock::now();
        _thread=start(this);
    }
    ~MCUAsync() {
        //Async<FPGA>::_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout<<"MCU counted "<<std::get<0>(_foreign.objects).getNumber()<<std::endl;
        std::cout<<"Dumping MCU DMA Objects"<<std::endl;
        dump_objects(_foreign.objects,_foreign.descriptors,boot_time,kill_time);
    }
    private:
    typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA> _dma;
    bool stateOfObjectZero = false;
    bool syncStateObjectZero = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};