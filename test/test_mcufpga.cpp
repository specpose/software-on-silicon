#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MCUFPGA.hpp"
#include <limits>
#include <bitset>

#define DMA std::array<char,std::numeric_limits<unsigned char>::max()-2>
DMA com_buffer;

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
        com_buffer[writePos++]=*reinterpret_cast<char*>(&out);
        if (writePos==com_buffer.size())
            writePos=0;
    }
    bool read(char r){
        std::bitset<24> temp{ reinterpret_cast<std::bitset<sizeof(char) * 8>*>(&r)->to_ullong()};
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
    unsigned int writePos = 0;
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
class FPGA : public SOS::Behavior::BiDirectionalController<SOS::Behavior::DummyController>, public SOS::Behavior::Loop, private Serial {
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
                data = '*';
            else
                data = '_';
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
            write('?');//empty write
        }
        std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
        }
        stop_token.getAcknowledgeRef().clear();
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
        int read4minus1 = 0;
        while(stop_token.getUpdatedRef().test_and_set()){
            char data = com_buffer[readPos++];
            if (readPos==com_buffer.size())
                readPos=0;
            while(read(data)){
            }
            auto read3bytes = read_flush();
            for(int i=0;i<3;i++)
                std::cout<<read3bytes[i];
        }
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    unsigned int readPos = 0;
    DMA hostMirror;
    std::thread _thread = std::thread{};
};

int main () {
    MCUThread host;
}
