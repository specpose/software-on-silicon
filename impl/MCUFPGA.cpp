#include <iostream>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/Serial.hpp"
#define COM_BUFFER std::array<unsigned char,1>
#include "software-on-silicon/MCUFPGA.hpp"
#include "software-on-silicon/mcufpga_helpers.hpp"
#include <limits>
#define DMA std::array<unsigned char,999>//1001%3=2

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
class FPGAProcessingSwitch : public SOS::Behavior::SerialProcessing, public SOS::Behavior::DummyEventController<> {
    public:
    using bus_type = typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA>;
    FPGAProcessingSwitch(bus_type& bus) : _nBus(bus), SOS::Behavior::SerialProcessing(), SOS::Behavior::DummyEventController<>(bus.signal) {
        _thread=SOS::Behavior::Loop::start(this);
    }
    ~FPGAProcessingSwitch(){
        _thread.detach();
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    void read_notify_hook(){
        if (!_nBus.com_shutdown) {
        auto object_id = std::get<0>(_nBus.cables).getReadDestinationRef();
        switch(object_id){
            case 0:
            //std::cout<<"FPGA received: "<<object_id<<std::endl;//fresh out of read_lock, safe before unsynced
            if (!std::get<0>(_nBus.objects).mcu_owned()){//WRITE-LOCK encapsulated <= Not all implementations need a write-lock
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
    }
    void write_notify_hook(){
        if (!_nBus.com_shutdown) {
        auto object_id = std::get<0>(_nBus.cables).getWriteOriginRef();
        switch(object_id){
            case 0:
            //std::cout<<"FPGA written: "<<object_id<<std::endl;//just been synced, can now process further
            break;
            case 1:
            //std::cout<<"FPGA written: "<<object_id<<std::endl;
            break;
            case 2:
            //std::cout<<"FPGA written: "<<object_id<<std::endl;
            break;
        }
        }
    }
    protected:
    virtual bool received() final {
        return !_intrinsic.getUpdatedRef().test_and_set();
    }
    virtual bool transfered() final {
        return !_intrinsic.getAcknowledgeRef().test_and_set();
    }
    virtual bool is_running() final { return SOS::Behavior::Loop::is_running(); }
    virtual void finished() final { SOS::Behavior::Loop::finished(); }
    private:
    bus_type& _nBus;
    std::thread _thread = std::thread{};
};
class MCUProcessingSwitch : public SOS::Behavior::SerialProcessing, public SOS::Behavior::DummyEventController<> {
    public:
    using bus_type = typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA>;
    MCUProcessingSwitch(bus_type& bus) : _nBus(bus), SOS::Behavior::SerialProcessing(), SOS::Behavior::DummyEventController<>(bus.signal) {
        _thread=SOS::Behavior::Loop::start(this);
    }
    ~MCUProcessingSwitch(){
        _thread.detach();
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    void read_notify_hook(){
        if (!_nBus.com_shutdown) {
        auto object_id = std::get<0>(_nBus.cables).getReadDestinationRef();
        switch(object_id){
            case 0:
            //std::cout<<"MCU received: "<<object_id<<std::endl;
            if (std::get<0>(_nBus.objects).mcu_owned()){//WRITE-LOCK encapsulated <= Not all implementations need a write-lock
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
    }
    void write_notify_hook(){
        if (!_nBus.com_shutdown) {
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
    }
    protected:
    virtual bool received() final {
        return !_intrinsic.getUpdatedRef().test_and_set();
    }
    virtual bool transfered() final {
        return !_intrinsic.getAcknowledgeRef().test_and_set();
    }
    virtual bool is_running() final { return SOS::Behavior::Loop::is_running(); }
    virtual void finished() final { SOS::Behavior::Loop::finished(); }
    private:
    bus_type& _nBus;
    std::thread _thread = std::thread{};
};
class FPGA : public SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, SymbolRateCounter, DMA, DMA> {
    public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    FPGA(bus_type& myBus) :
    SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, SymbolRateCounter, DMA, DMA>(myBus)
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
        //_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        //stop_token.getUpdatedRef().clear();
        //_thread.join();
        _thread.detach();
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout<<"FPGA counted "<<std::get<0>(_foreign.objects).getNumber()<<std::endl;
        std::cout<<"Dumping FPGA DMA Objects"<<std::endl;
        dump_objects(_foreign.objects,_foreign.descriptors,boot_time,kill_time);
    }
    void requestStop(){
        request_stop();
    };
    bool isStopped(){
        if (is_finished()){
            finished();
            return true;
        }
        return false;
    }
    private:
    bool stateOfObjectOne = false;
    bool syncStateObjectOne = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};
class MCU : public SOS::Behavior::SimulationMCU<MCUProcessingSwitch, SymbolRateCounter, DMA, DMA> {
    public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    MCU(bus_type& myBus) :
    SOS::Behavior::SimulationMCU<MCUProcessingSwitch, SymbolRateCounter, DMA, DMA>(myBus) {
        std::get<2>(_foreign.objects).fill('-');
        _foreign.descriptors[2].synced=false;
        boot_time = std::chrono::high_resolution_clock::now();
        _thread=start(this);
    }
    ~MCU() {
        ////Async<FPGA>::_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        //stop_token.getUpdatedRef().clear();
        //_thread.join();
        _thread.detach();
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout<<"MCU counted "<<std::get<0>(_foreign.objects).getNumber()<<std::endl;
        std::cout<<"Dumping MCU DMA Objects"<<std::endl;
        dump_objects(_foreign.objects,_foreign.descriptors,boot_time,kill_time);
    }
    bool isStopped(){
        if (is_finished()){
            finished();
            return true;
        }
        return false;
    };
    private:
    bool stateOfObjectZero = false;
    bool syncStateObjectZero = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};