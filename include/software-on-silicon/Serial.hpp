#include <bitset>

namespace SOS {
    namespace Protocol {
        class Serial {//write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
            public:
            void write(unsigned char w){
                std::bitset<8> out;
                switch (writeCount) {
                    case 0:
                        out = write_assemble(w);
                        out = write_recover(out,false);
                        writeCount++;
                    break;
                    case 1:
                        out = write_assemble(w);
                        out = write_recover(out);//recover last 1 2bit
                        writeCount++;
                    break;
                    case 2://call 3
                        out = write_assemble(w);
                        out = write_recover(out);//recover last 2 2bit
                        writeCount++;
                    break;
                    case 3:
                        out = write_assemble(w, false);
                        out = write_recover(out);;//recover 3 2bit from call 3 only
                        writeCount=0;
                    break;
                }
                com_buffer[writePos++]=static_cast<unsigned char>(out.to_ulong());
                if (writePos==com_buffer.size())
                    writePos=0;
            }
            bool read(unsigned char r){
                std::bitset<24> temp{ static_cast<unsigned long>(r)};
                if (temp[7])
                    fpga_updated.clear();
                if (temp[6])
                    mcu_acknowledge.clear();
                switch (readCount) {
                    case 0:
                    read_shift(temp);
                    readCount++;
                    return true;
                    case 1:
                    read_shift(temp);
                    readCount++;
                    return true;
                    case 2:
                    read_shift(temp);
                    readCount++;
                    return true;
                    case 3:
                    read_shift(temp);
                    readCount=0;
                }
                return false;
            }
            std::array<unsigned char,3> read_flush(){
                std::array<unsigned char, 3> result;// = *reinterpret_cast<std::array<unsigned char, 3>*>(&(in[23]));//wrong bit-order
                result[0] = static_cast<unsigned char>((readAssembly >> 16).to_ulong());
                result[1] = static_cast<unsigned char>(((readAssembly << 8)>> 16).to_ulong());
                result[2] = static_cast<unsigned char>(((readAssembly << 16) >> 16).to_ulong());
                readAssembly.reset();
                return result;
            }
            private:
            unsigned int writePos = 0;
            unsigned int writeCount = 0;
            unsigned int readCount = 0;
            std::atomic_flag mcu_updated;//write bit 0
            std::atomic_flag fpga_acknowledge;//write bit 1
            std::atomic_flag fpga_updated;//read bit 0
            std::atomic_flag mcu_acknowledge;//read bit 1
            std::array<std::bitset<8>,3> writeAssembly;
            std::bitset<24> readAssembly;
            std::bitset<8> write_assemble(unsigned char w,bool assemble=true){
                std::bitset<8> out;
                if (assemble){
                    writeAssembly[writeCount]=w;
                    out = writeAssembly[writeCount]>>(writeCount+1)*2;
                }
                if (!mcu_updated.test_and_set()){
                    mcu_updated.clear();
                    out.set(7,1);
                }
                if (!fpga_acknowledge.test_and_set()){
                    fpga_acknowledge.clear();
                    out.set(6,1);
                }
                return out;
            }
            std::bitset<8> write_recover(std::bitset<8>& out,bool recover=true){
                std::bitset<8> cache;
                if (recover){
                    cache = writeAssembly[writeCount-1]<<(4-writeCount)*2;
                    cache = cache>>1*2;
                }
                return out^cache;
            }
            void read_shift(std::bitset<24>& temp){
                temp = temp<<(4-0)*4+2;//split off 1st 2bit
                temp = temp>>(readCount*3)*2;//shift
                readAssembly = readAssembly ^ temp;//overlay
            }
        };
    }
}