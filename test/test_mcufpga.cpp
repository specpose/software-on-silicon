#include <iostream>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MCUFPGA.hpp"
#include <limits>

#define DMA std::array<unsigned char,999>//1001%3=2
DMA com_buffer;
#include "software-on-silicon/Serial.hpp"

using namespace SOS::MemoryView;

class FPGA : public SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>, public SOS::Behavior::Loop, private SOS::Protocol::SerialFPGA<DMA> {
    public:
    using bus_type = WriteLock;
    FPGA(bus_type& myBus) :
    SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>::BiDirectionalController(myBus.signal),
    Loop(),
    SOS::Protocol::SerialFPGA<DMA>() {
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
        //for (std::size_t i=0;i<std::get<0>(objects).size();i++)
        //    printf("%c",std::get<0>(objects)[i]);
        descriptors[0].synced=false;
        _thread=start(this);
    }
    ~FPGA() {
        //_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        _thread.join();
    }
    void event_loop(){
        int write3plus1 = 0;
        while(stop_token.getUpdatedRef().test_and_set()){
            const auto start = high_resolution_clock::now();
            write_hook(write3plus1);
            std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
        }
        stop_token.getAcknowledgeRef().clear();
    }
    protected:
    private:
    std::thread _thread = std::thread{};
};
class MCUThread : public Thread<FPGA>, public SOS::Behavior::Loop, public SOS::Protocol::SerialMCU<DMA> {
    public:
    MCUThread() : Thread<FPGA>(), Loop(), SOS::Protocol::SerialMCU<DMA>() {
        _thread=start(this);
    }
    ~MCUThread() {
        _child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
    }
    void event_loop(){
        int read4minus1 = 0;
        while(stop_token.getUpdatedRef().test_and_set()){
            read_hook(read4minus1);
            std::this_thread::yield();
        }
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    std::thread _thread = std::thread{};
};

int main () {
    auto host= MCUThread();
    const auto start = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < 1) {
        std::this_thread::yield();
    }
    host.stop();
    for (std::size_t i=0;i<std::get<0>(host.objects).size();i++){
        printf("%c",std::get<0>(host.objects)[i]);
    }
}
