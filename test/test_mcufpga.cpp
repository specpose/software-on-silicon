#include <iostream>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MCUFPGA.hpp"
#include <limits>

#define DMA std::array<char,std::numeric_limits<unsigned char>::max()-2>

using namespace SOS::MemoryView;
class Serial {
    public:
    void write(char){}
    char read(){return 0;}
};
class FPGA : public SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>, public SOS::Behavior::Loop {
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
    }
    private:
    DMA embeddedMirror;
    std::thread _thread = std::thread{};
};
class MCUThread : public Thread<FPGA>, public SOS::Behavior::Loop, private Serial {
    public:
    MCUThread() : Thread<FPGA>(), Loop() {
        _thread=start(this);
    }
    ~MCUThread() {
        _child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        _thread.join();
    }
    void event_loop(){
    }
    private:
    DMA hostMirror;
    std::thread _thread = std::thread{};
};

int main () {
    MCUThread host;
}