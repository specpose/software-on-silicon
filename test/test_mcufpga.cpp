#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MCUFPGA.hpp"
#include <limits>
#include <bitset>

#define DMA std::array<char,std::numeric_limits<unsigned char>::max()-2>

using namespace SOS::MemoryView;
class Serial {//write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
    public:
    void write(char w){
        std::bitset<8> out = write_assemble(w);
        switch (writeCount) {
            case 0:
            writeCount++;
            break;
            case 1:
                out = write_recover(out);//recover last 1 2bit
                writeCount++;
            break;
            case 2://call 3
                out = write_recover(out);//recover last 2 2bit
                writeCount++;
            break;
            case 3:
                out = write_recover(out);;//recover 3 2bit from call 3 only
                writeCount=0;
            break;
        }
    }
    bool read(char r){
        std::bitset<24> temp{r};
        if (temp[16])
            fpga_updated.clear();
        if (temp[17])
            mcu_acknowledge.clear();
        std::bitset<24> in;
        switch (readCount) {
            case 0:
            read_shift(temp);
            readCount++;
            return true;
            //break;
            case 1:
            read_shift(temp);
            readCount++;
            return true;
            //break;
            case 2:
            read_shift(temp);
            readCount++;
            return true;
            //break;
            case 3:
            read_shift(temp);
            readCount=0;
            //break;
        }
        return false;
    }
    std::array<char,3> read_flush(){
        std::array<char,3> result = *reinterpret_cast<std::array<char,3>*>(&in);
        return result;
    }
    private:
    unsigned int writeCount = 0;
    unsigned int readCount = 0;
    std::bitset<24> in;
    std::atomic_flag mcu_updated;//write bit 0
    std::atomic_flag fpga_acknowledge;//write bit 1
    std::atomic_flag fpga_updated;//read bit 0
    std::atomic_flag mcu_acknowledge;//read bit 1
    std::array<std::bitset<8>,3> writeAssembly;
    std::array<std::bitset<24>,4> readAssembly;
    std::bitset<8> write_assemble(char w){
        std::bitset<8> out;
        writeAssembly[writeCount]=w;
        out = writeAssembly[writeCount]>>(writeCount+1)*2;
        if (!mcu_updated.test_and_set()){
            mcu_updated.clear();
            out.set(0,1);
        }
        if (!fpga_acknowledge.test_and_set()){
            fpga_acknowledge.clear();
            out.set(1,1);
        }
        return out;
    }
    std::bitset<8> write_recover(std::bitset<8>& out){
        std::bitset<8> cache;
        cache = writeAssembly[writeCount-1]<<(4-writeCount)*2;//recover last 1 2bit
        cache = cache>>1*2;
        return out^cache;
    }
    void read_shift(std::bitset<24>& temp){
        temp = temp<<(4-0)*4+2;//split off 1st 2bit
        temp = temp>>(readCount*3)*2;//shift
        in = in ^ temp;//overlay
    }
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
        while(stop_token.getUpdatedRef().test_and_set()){
        }
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    DMA hostMirror;
    std::thread _thread = std::thread{};
};

int main () {
    MCUThread host;
}