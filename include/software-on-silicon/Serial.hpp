#include <bitset>

namespace SOS {
    namespace Protocol {
        struct DMADescriptor {
            DMADescriptor(){}//DANGER
            DMADescriptor(unsigned char id, void* obj, std::size_t obj_size) : id(id),obj(obj),obj_size(obj_size){
                //std::cout<<obj_size<<" mod "<<" 3 ="<<obj_size%3<<std::endl;
                //if (obj_size%3!=2)//1 byte for object index
                if (obj_size%3!=0)
                    throw std::logic_error("Invalid DMAObject size");
            }
            unsigned char id = 0xFF;
            void* obj = nullptr;
            std::size_t obj_size = 0;
            bool readLock = false;//serial priority checks for readLock; subcontroller<subcontroller> read checks for readLock
            bool synced = true;//subcontroller write checks for synced
        };
        template<unsigned int N> struct DescriptorHelper : public std::array<DMADescriptor,N> {
            public:
            using std::array<DMADescriptor,N>::array;
            template<typename... T> void operator()(T&... obj_ref){
                (assign(obj_ref),...);
            }
            private:
            template<typename T> void assign(T& obj_ref){
                (*this)[count] = DMADescriptor(static_cast<unsigned char>(count),reinterpret_cast<void*>(&obj_ref),sizeof(obj_ref));
                count++;
            }
            std::size_t count = 0;
        };
        template<typename... Objects> class Serial {//write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
            public:
            Serial(){
                std::apply(descriptors,objects);//ALWAYS: Initialize Descriptors in Constructor
            }
            protected:
            void read_hook(int& read4minus1){
                unsigned char data = com_buffer[readPos++];
                if (readPos==com_buffer.size())
                    readPos=0;
                read(data);
                if(!receive_lock){
                for (std::size_t i=0;i<descriptors.size();i++){
                    if (descriptors[i].synced==true){//thread receive_id=byte[0]
                        receive_lock=true;
                        descriptors[i].readLock=true;
                        readDestination=0;//HARDCODED: object[0].id
                        readDestinationPos=0;
                        send_acknowledge();
                    }
                }
                }
                if(receive_lock){
                    if (readDestinationPos==descriptors[readDestination].obj_size){//byte[0] => obj_size+1
                        descriptors[readDestination].readLock=false;
                        receive_lock=false;
                        readDestinationPos=0;
                    }
                    if (read4minus1<3){
                        read4minus1++;
                    } else if (read4minus1==3){
                        auto read3bytes = read_flush();
                        for(std::size_t i=0;i<3;i++){
                            reinterpret_cast<char*>(descriptors[readDestination].obj)[readDestinationPos++]=read3bytes[i];//byte[0] => obj_size+1
                            //printf("%c",read3bytes[i]);
                        }
                        read4minus1 = 0;
                    }
                }
            }
            void write_hook(int& write3plus1){
                if (!send_lock){
                bool gotOne = false;
                for (std::size_t i=0;i<descriptors.size();i++){
                    if (!descriptors[i].readLock && !descriptors[i].synced && !gotOne){
                        send_lock=true;
                        send_request();
                        writeOrigin=descriptors[i].id;
                        writeOriginPos=0;
                        gotOne=true;
                    }
                }
                }
                if (send_lock) {//PROBLEM? not acquiring lock from mcu if nothing to write
                if (write3plus1<3){
                    unsigned char data = reinterpret_cast<char*>(descriptors[writeOrigin].obj)[writeOriginPos++];
                    write(data);
                    write3plus1++;
                } else if (write3plus1==3){
                    if (writeOriginPos==descriptors[writeOrigin].obj_size){
                        //throw std::runtime_error("WRITE COMPLETED");
                        //std::cout<<std::endl<<"WRITE COMPLETED "<<writeOriginPos<<std::endl;
                        descriptors[writeOrigin].synced=true;
                        send_lock=false;
                        writeOriginPos=0;
                    }
                    write(63);//'?' empty write
                    write3plus1=0;
                }
                }
            }
            virtual void send_acknowledge() = 0;
            virtual void send_request() = 0;
            std::atomic_flag mcu_updated;//mcu_write,fpga_read bit 7
            std::atomic_flag fpga_acknowledge;//mcu_write,fpga_read bit 6
            std::atomic_flag fpga_updated;//mcu_read,fpga_write bit 7
            std::atomic_flag mcu_acknowledge;//mcu_read,fpga_write bit 6
            //protected:
            public:
            std::tuple<Objects...> objects{};
            protected:
            //private:
            DescriptorHelper<std::tuple_size<std::tuple<Objects...>>::value> descriptors{};
            private:
            bool receive_lock = false;
            bool send_lock = false;
            std::size_t writePos = 0;//REPLACE: com_buffer
            unsigned int writeCount = 0;//write3plus1
            std::size_t writeOrigin = 0;//HARDCODED: objects[0]
            std::size_t writeOriginPos = 0;
            std::size_t readPos = 0;//REPLACE: com_buffer
            unsigned int readCount = 0;//read4minus1
            std::size_t readDestination = 0;//HARDCODED: objects[0]
            std::size_t readDestinationPos = 0;
            std::array<std::bitset<8>,3> writeAssembly;
            std::bitset<24> readAssembly;
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
        template<typename... Objects> class SerialMCU : public Serial<Objects...> {
            private:
            virtual void read_bits(std::bitset<24>& temp) final {
                if (temp[7])
                    Serial<Objects...>::fpga_updated.clear();
                else
                    Serial<Objects...>::fpga_updated.test_and_set();
                if (temp[6])
                    Serial<Objects...>::mcu_acknowledge.clear();
                else
                    Serial<Objects...>::mcu_acknowledge.test_and_set();
            }
            virtual void write_bits(std::bitset<8>& out) final {
                if (!Serial<Objects...>::mcu_updated.test_and_set()){
                    Serial<Objects...>::mcu_updated.clear();
                    out.set(7,1);
                }
                if (!Serial<Objects...>::fpga_acknowledge.test_and_set()){
                    Serial<Objects...>::fpga_acknowledge.clear();
                    out.set(6,1);
                }
            }
            virtual void send_acknowledge() final {//PROBLEM? Needs MCUPriority?
                if (!Serial<Objects...>::fpga_updated.test_and_set()){
                    Serial<Objects...>::fpga_acknowledge.clear();
                }
            }
            virtual void send_request() final {
                Serial<Objects...>::mcu_updated.clear();
            }
        };
        template<typename... Objects> class SerialFPGA : public Serial<Objects...> {
            private:
            virtual void read_bits(std::bitset<24>& temp) final {
                if (temp[7])
                    Serial<Objects...>::mcu_updated.clear();
                else
                    Serial<Objects...>::mcu_updated.test_and_set();
                if (temp[6])
                    Serial<Objects...>::fpga_acknowledge.clear();
                else
                    Serial<Objects...>::fpga_acknowledge.test_and_set();
            }
            virtual void write_bits(std::bitset<8>& out) final {
                if (!Serial<Objects...>::fpga_updated.test_and_set()){
                    Serial<Objects...>::fpga_updated.clear();
                    out.set(7,1);
                }
                if (!Serial<Objects...>::mcu_acknowledge.test_and_set()){
                    Serial<Objects...>::mcu_acknowledge.clear();
                    out.set(6,1);
                }
            }
            virtual void send_acknowledge() final {
                if (!Serial<Objects...>::mcu_updated.test_and_set()){
                    Serial<Objects...>::mcu_acknowledge.clear();
                }
            }
            virtual void send_request() final {
                Serial<Objects...>::fpga_updated.clear();
            }
        };
        /*struct DMADescriptor {
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
            constexpr DMADescriptors(){}//DANGER
            DMADescriptors(First& h, Others&... t)
            : m_head(sizeof...(Others),&h), inherited(t...) {}
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
        }*/
    }
}