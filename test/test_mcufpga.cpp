#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MCUFPGA.hpp"

#define DMA std::array<char,1000>

using namespace SOS::MemoryView;
class FPGA : public SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController> {
    public:
    using bus_type = WriteLock;
    FPGA(bus_type& myBus) : SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>::BiDirectionalController(myBus.signal) {}

    private:
    DMA embeddedMirror;
};

class MCUThread : public Thread<FPGA> {
    public:
    using Thread<FPGA>::Thread;

    private:
    DMA hostMirror;
};

int main () {
    MCUThread host;
}