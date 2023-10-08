#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MCUFPGA.hpp"
#include <limits>

#define DMA std::array<unsigned char,std::numeric_limits<unsigned char>::max()-2>
DMA com_buffer;
#include "software-on-silicon/Serial.hpp"

using namespace SOS::MemoryView;

class FPGA : public SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>, public SOS::Behavior::Loop, private SOS::Protocol::Serial {
    public:
    using bus_type = WriteLock;
    FPGA(bus_type& myBus) :
    SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>::BiDirectionalController(myBus.signal),
    Loop(){
        _thread=start(this);
    }
    ~FPGA() {
        //_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        _thread.join();
    }
    void event_loop(){
        int write3plus1 = 0;
        int counter = 0;
        bool blink = true;
        while(stop_token.getUpdatedRef().test_and_set()){
        const auto start = high_resolution_clock::now();
        if (write3plus1<3){
            DMA::value_type data;
            if (blink)
                data = 42;//'*'
            else
                data = 95;//'_'
            write(data);
            counter++;
            if (blink && counter==333){
                blink = false;
                counter=0;
            } else if (!blink && counter==666) {
                blink = true;
                counter=0;
            }
            write3plus1++;
        } else if (write3plus1==3){
            write(63);//'?' empty write
        }
        std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
        }
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    DMA embeddedMirror;
    std::thread _thread = std::thread{};
};
class MCUThread : public Thread<FPGA>, public SOS::Behavior::Loop, private SOS::Protocol::Serial {
    public:
    MCUThread() : Thread<FPGA>(), Loop() {
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
            unsigned char data = com_buffer[readPos++];
            if (readPos==com_buffer.size())
                readPos=0;
            while(read(data)){
            }
            auto read3bytes = read_flush();
            for(int i=0;i<3;i++)
                printf("%c",read3bytes[i]);
            std::this_thread::yield();
        }
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    unsigned int readPos = 0;
    DMA hostMirror;
    std::thread _thread = std::thread{};
};

int main () {
    auto host= MCUThread();
    const auto start = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < 5) {
        std::this_thread::yield();
    }
    host.stop();
}
