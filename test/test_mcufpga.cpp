#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MCUFPGA.hpp"
#include <limits>

#define DMA std::array<unsigned char,std::numeric_limits<unsigned char>::max()-1>//255%3=0
DMA com_buffer;
#include "software-on-silicon/Serial.hpp"

using namespace SOS::MemoryView;

class FPGA : public SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>, public SOS::Behavior::Loop, private SOS::Protocol::SerialFPGA {
    public:
    using bus_type = WriteLock;
    FPGA(bus_type& myBus) :
    SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>::BiDirectionalController(myBus.signal),
    Loop() {
        descriptors=SOS::Protocol::DMADescriptors<DMA>(std::get<0>(objects));
        //SOS::Protocol::get(descriptors,0).id = 0;//ALWAYS number descriptors manually
        //SOS::Protocol::get(descriptors,0).obj = &std::get<0>(objects);//ALWAYS associate all descriptors with objects
        _thread=start(this);
    }
    ~FPGA() {
        //_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        _thread.join();
    }
    void event_loop(){
        while(stop_token.getUpdatedRef().test_and_set()){
            const auto start = high_resolution_clock::now();
            if (write3plus1<3){
                DMA::value_type data;
                if (writeBlink)
                    data = 42;//'*'
                else
                    data = 95;//'_'
                SOS::Protocol::SerialFPGA::write(data);
                writeBlinkCounter++;
                if (writeBlink && writeBlinkCounter==333){
                    writeBlink = false;
                    writeBlinkCounter=0;
                } else if (!writeBlink && writeBlinkCounter==666) {
                    writeBlink = true;
                    writeBlinkCounter=0;
                }
                write3plus1++;
            } else if (write3plus1==3){
                SOS::Protocol::SerialFPGA::write(63);//'?' empty write
                write3plus1=0;
            }
            std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
        }
        stop_token.getAcknowledgeRef().clear();
    }
    protected:
    SOS::Protocol::DMADescriptors<DMA> descriptors;
    private:
    int write3plus1 = 0;
    int writeBlinkCounter = 0;
    bool writeBlink = true;
    std::tuple<DMA> objects = std::tuple<DMA>{};
    std::thread _thread = std::thread{};
};
class MCUThread : public Thread<FPGA>, public SOS::Behavior::Loop, private SOS::Protocol::SerialMCU {
    public:
    MCUThread() : Thread<FPGA>(), Loop() {
        descriptors=SOS::Protocol::DMADescriptors<DMA>(std::get<0>(objects));
        //SOS::Protocol::get(descriptors,0).id = 0;//ALWAYS number descriptors manually
        //SOS::Protocol::get(descriptors,0).obj = &std::get<0>(objects);//ALWAYS associate all descriptors with objects
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
            SOS::Protocol::SerialMCU::read(data);
            if (read4minus1<3){
                read4minus1++;
            } else if (read4minus1==3){
                auto read3bytes = read_flush();
                for(int i=0;i<3;i++)
                    printf("%c",read3bytes[i]);
                read4minus1 = 0;
            }
            std::this_thread::yield();
        }
        stop_token.getAcknowledgeRef().clear();
    }
    protected:
    SOS::Protocol::DMADescriptors<DMA> descriptors;
    private:
    unsigned int readPos = 0;
    std::tuple<DMA> objects = std::tuple<DMA>{};
    std::thread _thread = std::thread{};
};

int main () {
    auto host= MCUThread();
    const auto start = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now() - start).count() < 1) {
        std::this_thread::yield();
    }
    host.stop();
}
