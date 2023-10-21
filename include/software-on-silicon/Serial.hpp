#include <bitset>

namespace SOS {
    namespace Protocol {
        class Serial {//write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
            public:
            void write(unsigned char w){
                std::bitset<8> out;
                switch (writeCount) {
                    case 0:
                        out = write_assemble(writeAssembly, writeCount, w);
                        write_bits(out);
                        writeCount++;
                    break;
                    case 1:
                        out = write_assemble(writeAssembly, writeCount, w);
                        write_bits(out);
                        out = write_recover(writeAssembly, writeCount, out);//recover last 1 2bit
                        writeCount++;
                    break;
                    case 2://call 3
                        out = write_assemble(writeAssembly, writeCount,w);
                        write_bits(out);
                        out = write_recover(writeAssembly, writeCount, out);//recover last 2 2bit
                        writeCount++;
                    break;
                    case 3:
                        write_bits(out);
                        out = write_recover(writeAssembly, writeCount, out);;//recover 3 2bit from call 3 only
                        writeCount=0;
                    break;
                }
                com_buffer[writePos++]=static_cast<unsigned char>(out.to_ulong());
                if (writePos==com_buffer.size())
                    writePos=0;
            }
            bool read(unsigned char r){
                std::bitset<24> temp{ static_cast<unsigned long>(r)};
                read_bits(temp);
                switch (readCount) {
                    case 0:
                    read_shift(readAssembly, readCount, temp);
                    readCount++;
                    return true;
                    case 1:
                    read_shift(readAssembly, readCount, temp);
                    readCount++;
                    return true;
                    case 2:
                    read_shift(readAssembly, readCount, temp);
                    readCount++;
                    return true;
                    case 3:
                    read_shift(readAssembly, readCount, temp);
                    readCount=0;
                }
                return false;
            }
            std::array<unsigned char,3> read_flush(){
                std::array<unsigned char, 3> result;// = *reinterpret_cast<std::array<unsigned char, 3>*>(&(readAssembly[0]));//wrong bit-order
                result[0] = static_cast<unsigned char>((readAssembly >> 16).to_ulong());
                result[1] = static_cast<unsigned char>(((readAssembly << 8)>> 16).to_ulong());
                result[2] = static_cast<unsigned char>(((readAssembly << 16) >> 16).to_ulong());
                readAssembly.reset();
                return result;
            }
            protected:
            std::atomic_flag mcu_updated;//mcu_write,fpga_read bit 7
            std::atomic_flag fpga_acknowledge;//mcu_write,fpga_read bit 6
            std::atomic_flag fpga_updated;//mcu_read,fpga_write bit 7
            std::atomic_flag mcu_acknowledge;//mcu_read,fpga_write bit 6
            private:
            unsigned int writePos = 0;
            unsigned int writeCount = 0;
            unsigned int readCount = 0;
            std::array<std::bitset<8>,3> writeAssembly;
            std::bitset<24> readAssembly;
            static std::bitset<8> write_assemble(decltype(writeAssembly)& writeAssembly,decltype(writeCount)& writeCount, unsigned char w) {
                std::bitset<8> out;
                writeAssembly[writeCount]=w;
                out = writeAssembly[writeCount]>>(writeCount+1)*2;
                return out;
            }
            virtual void write_bits(std::bitset<8>& out) = 0;
            static std::bitset<8> write_recover(decltype(writeAssembly)& writeAssembly,decltype(writeCount)& writeCount, std::bitset<8>& out) {
                std::bitset<8> cache;
                cache = writeAssembly[writeCount-1]<<(4-writeCount)*2;
                cache = cache>>1*2;
                return out^cache;
            }
            virtual void read_bits(std::bitset<24>& temp) = 0;
            static void read_shift(decltype(readAssembly)& readAssembly,decltype(readCount)& readCount, std::bitset<24>& temp){
                temp = temp<<(4-0)*4+2;//split off 1st 2bit
                temp = temp>>(readCount*3)*2;//shift
                readAssembly = readAssembly ^ temp;//overlay
            }
        };
        class SerialMCU : public Serial {
            private:
            virtual void read_bits(std::bitset<24>& temp) final {
                if (temp[7])
                    fpga_updated.clear();
                else
                    fpga_updated.test_and_set();
                if (temp[6])
                    mcu_acknowledge.clear();
                else
                    mcu_acknowledge.test_and_set();
            }
            virtual void write_bits(std::bitset<8>& out) final {
                if (!mcu_updated.test_and_set()){
                    mcu_updated.clear();
                    out.set(7,1);
                }
                if (!fpga_acknowledge.test_and_set()){
                    fpga_acknowledge.clear();
                    out.set(6,1);
                }
            }
        };
        class SerialFPGA : public Serial {
            private:
            virtual void read_bits(std::bitset<24>& temp) final {
                if (temp[7])
                    mcu_updated.clear();
                else
                    mcu_updated.test_and_set();
                if (temp[6])
                    fpga_acknowledge.clear();
                else
                    fpga_acknowledge.test_and_set();
            }
            virtual void write_bits(std::bitset<8>& out) final {
                if (!fpga_updated.test_and_set()){
                    fpga_updated.clear();
                    out.set(7,1);
                }
                if (!mcu_acknowledge.test_and_set()){
                    mcu_acknowledge.clear();
                    out.set(6,1);
                }
            }
        };
        struct DMADescriptor {
            DMADescriptor(){}//DANGER
            DMADescriptor(unsigned char id, void* obj) : id(id),obj(obj){}
            unsigned char id;
            void* obj;
        };
        //Stroustrup 28.6.4
        template<typename... Objects> class DMADescriptors;
        template<> class DMADescriptors<> {};//DANGER
        template<typename First, typename... Others> class DMADescriptors<First, Others...> : private DMADescriptors<Others...> {
            typedef DMADescriptors<Others...> inherited;
            public:
            constexpr DMADescriptors(){}
            DMADescriptors(First h, Others... t)
            : m_head(sizeof...(Others),nullptr), inherited(t...) {}
            template<typename... Objects> DMADescriptors(const DMADescriptors<Objects...>& other)
            : m_head(other.head()), inherited(other.tail()) {}
            template<typename... Objects> DMADescriptors& operator=(const DMADescriptors<Objects...>& other){
                m_head=other.head();
                tail()=other.tail();
                return *this;
            }
            //private:
            DMADescriptor& head(){return m_head;};
            const DMADescriptor& head() const {return m_head;};
            inherited& tail(){return *this;};
            const inherited& tail() const {return *this;};
            protected:
            DMADescriptor m_head;
        };
        template<unsigned char N, typename... T>
        DMADescriptor& get(DMADescriptors<T...>& t) {
            if constexpr(sizeof...(T)!=0){
                if constexpr(N < sizeof...(T)){
                    //if ((t.head().id-(sizeof...(T)-1))==id){
                    if ((t.head().id)==N){
                        return t.head();
                    }
                    return get<N>(t.tail());
                } else {
                    throw std::runtime_error("DMADescriptor does not exist");
                }
            } else {
                throw std::logic_error("get<> can not be used with empty DMADescriptors<>");
            }
        }
        template<typename... Objects> constexpr std::size_t DMADescriptors_size(){ return sizeof...(Objects); }
        //https://stackoverflow.com/questions/1198260/how-can-you-iterate-over-the-elements-of-an-stdtuple
        template<typename... T>
        DMADescriptor& get(DMADescriptors<T...>& t,unsigned char id) {
            if constexpr(sizeof...(T)!=0){
                if (id < sizeof...(T)){
                    //if ((t.head().id-(sizeof...(T)-1))==id){
                    if ((t.head().id)==id){
                        return t.head();
                    }
                    return get(t.tail(),id);
                } else {
                    throw std::runtime_error("DMADescriptor does not exist");
                }
            } else {
                throw std::logic_error("get<> can not be used with empty DMADescriptors<>");
            }
        }
    }
}