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
class CounterTask {
    public:
    CounterTask(){}
};
class FPGAProcessingSwitch : private CounterTask {
    public:
    using process_notifier_ct = typename std::tuple_element<0,typename SOS::MemoryView::SerialProcessNotifier::const_cables_type>::type;
    FPGAProcessingSwitch(process_notifier_ct& notifier) : _notifier(notifier), CounterTask() {}
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
class MCUProcessingSwitch: private CounterTask {
    public:
    using process_notifier_ct = typename std::tuple_element<0,typename SOS::MemoryView::SerialProcessNotifier::const_cables_type>::type;
    MCUProcessingSwitch(process_notifier_ct& notifier) : _notifier(notifier), CounterTask() {}
    void read_notify_hook(){
        auto object_id = _notifier.getReadDestinationRef().load();
        switch(object_id){
            case 0:
            std::cout<<"MCU received object id: "<<object_id<<std::endl;
            break;
            case 1:
            std::cout<<"MCU received object id: "<<object_id<<std::endl;
            break;
            case 2:
            std::cout<<"MCU received object id: "<<object_id<<std::endl;
            break;
        }
    }
    void write_notify_hook(){
        auto object_id = _notifier.getWriteOriginRef().load();
        switch(object_id){
            case 0:
            std::cout<<"MCU written object id: "<<object_id<<std::endl;
            break;
            case 1:
            std::cout<<"MCU written object id: "<<object_id<<std::endl;
            break;
            case 2:
            std::cout<<"MCU written object id: "<<object_id<<std::endl;
            break;
        }
    }
    private:
    process_notifier_ct& _notifier;
};
template<typename ProcessingSwitch, typename... Objects> class SerialProcessing :
protected ProcessingSwitch,
protected SOS::Behavior::DummyEventController<>,
public SOS::Behavior::Loop {
    public:
    using bus_type = SOS::MemoryView::SerialProcessNotifier;
    SerialProcessing(bus_type& bus, std::tuple<Objects...>& objects) :
    ProcessingSwitch(std::get<0>(bus.const_cables)),
    SOS::Behavior::DummyEventController<>(bus.signal),
    SOS::Behavior::Loop() {}
    void event_loop() {
        while(stop_token.getUpdatedRef().test_and_set()){
            if (!_intrinsic.getAcknowledgeRef().test_and_set()){
                ProcessingSwitch::write_notify_hook();
            }
            if (!_intrinsic.getUpdatedRef().test_and_set()){
                ProcessingSwitch::read_notify_hook();
            }
            std::this_thread::yield();
        }
        stop_token.getAcknowledgeRef().clear();
    }
};
template<typename ProcessingSwitch, typename... Objects> class SerialProcessingImpl : public SerialProcessing<ProcessingSwitch, Objects...> {
    public:
    SerialProcessingImpl(typename SerialProcessing<ProcessingSwitch>::bus_type& bus, std::tuple<Objects...>& objects) : SerialProcessing<ProcessingSwitch, Objects...>(bus, objects) {
        _thread=SOS::Behavior::Loop::start(this);
    }
    ~SerialProcessingImpl(){
        SerialProcessing<ProcessingSwitch, Objects...>::stop_token.getUpdatedRef().clear();
        _thread.join();
    }
    private:
    std::thread _thread = std::thread{};
};
/*template<typename... Objects> class ObjectBus : SOS::MemoryView::Bus {
    ObjectBus(std::tuple<Objects...>& objects) objects(objects) {}
    std::tuple<Objects...>& objects
};*/
class FPGA : private virtual SOS::Protocol::SerialFPGA<SerialProcessingImpl<FPGAProcessingSwitch, SymbolRateCounter,DMA,DMA>, SymbolRateCounter,DMA,DMA>,
public SOS::Behavior::SimulationFPGA<SerialProcessingImpl<FPGAProcessingSwitch, SymbolRateCounter,DMA,DMA>, SymbolRateCounter,DMA,DMA> {
    public:
    using bus_type = SOS::MemoryView::BusShaker;
    FPGA(bus_type& myBus) :
    SOS::Behavior::EventController<SerialProcessingImpl<FPGAProcessingSwitch, SymbolRateCounter,DMA,DMA>, std::tuple<SymbolRateCounter,DMA,DMA>>(myBus.signal,objects),
    SOS::Protocol::SerialFPGA<SerialProcessingImpl<FPGAProcessingSwitch, SymbolRateCounter,DMA,DMA>, SymbolRateCounter,DMA,DMA>(),
    SOS::Behavior::SimulationFPGA<SerialProcessingImpl<FPGAProcessingSwitch, SymbolRateCounter,DMA,DMA>, SymbolRateCounter,DMA,DMA>(mcu_to_fpga_buffer,fpga_to_mcu_buffer)
    {
        int writeBlinkCounter = 0;
        bool writeBlink = true;
        for (std::size_t i=0;i<std::get<1>(objects).size();i++){
            DMA::value_type data;
            if (writeBlink)
                data = 42;//'*'
            else
                data = 95;//'_'
            std::get<1>(objects)[i]=data;
            writeBlinkCounter++;
            if (writeBlink && writeBlinkCounter==333){
                writeBlink = false;
                writeBlinkCounter=0;
            } else if (!writeBlink && writeBlinkCounter==666) {
                writeBlink = true;
                writeBlinkCounter=0;
            }
        }
        descriptors[1].synced=false;
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
class MCUThread : private virtual SOS::Protocol::SerialMCU<SerialProcessingImpl<MCUProcessingSwitch, SymbolRateCounter,DMA,DMA>, SymbolRateCounter,DMA,DMA>,
public SOS::Behavior::SimulationMCU<SerialProcessingImpl<MCUProcessingSwitch, SymbolRateCounter,DMA,DMA>, SymbolRateCounter,DMA,DMA> {
    public:
    using bus_type = SOS::MemoryView::BusShaker;
    MCUThread(bus_type& myBus) :
    SOS::Behavior::EventController<SerialProcessingImpl<MCUProcessingSwitch, SymbolRateCounter,DMA,DMA>, std::tuple<SymbolRateCounter,DMA,DMA>>(myBus.signal,objects),
    SOS::Protocol::SerialMCU<SerialProcessingImpl<MCUProcessingSwitch, SymbolRateCounter,DMA,DMA>, SymbolRateCounter,DMA, DMA>(),
    SOS::Behavior::SimulationMCU<SerialProcessingImpl<MCUProcessingSwitch, SymbolRateCounter,DMA,DMA>, SymbolRateCounter,DMA,DMA>(fpga_to_mcu_buffer,mcu_to_fpga_buffer) {
        std::get<2>(objects).fill('-');
        descriptors[2].synced=false;
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