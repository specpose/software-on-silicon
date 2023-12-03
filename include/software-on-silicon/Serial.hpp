#include <bitset>

namespace SOS {
    namespace Protocol {
        static std::bitset<8> idleState() {//constexpr
            std::bitset<8> id;
            id.set(7,1);//updated==true
            id.set(6,0);//acknowledge==false
            //set 6bit data to "111111"
            for(std::size_t i = 0; i <= id.size()-1-2; i++){
                id.set(i,1);
            }
            return id;//-> "10111111"
        }
        struct DMADescriptor {
            DMADescriptor(){}//DANGER
            DMADescriptor(unsigned char id, void* obj, std::size_t obj_size) : id(id),obj(obj),obj_size(obj_size){
                if (obj_size%3!=0)
                    throw SFA::util::logic_error("Invalid DMAObject size",__FILE__,__func__);
                //check for "10111111" => >62
                auto maxId = static_cast<unsigned long>(((idleState()<<2)>>2).to_ulong());
                if (id==maxId)
                    throw SFA::util::logic_error("DMADescriptor id is reserved for the serial line idle state",__FILE__,__func__);
            }
            unsigned char id = 0xFF;
            void* obj = nullptr;
            std::size_t obj_size = 0;
            bool readLock = false;//serial priority checks for readLock; subcontroller<subcontroller> read checks for readLock
            bool synced = true;//subcontroller transfer checks for synced
            int rx_counter = 0;//DEBUG
            int tx_counter = 0;//DEBUG
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
        template<typename... Objects> class Serial : public SOS::Behavior::Loop {//write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
            public:
            Serial() : SOS::Behavior::Loop() {
                std::apply(descriptors,objects);//ALWAYS: Initialize Descriptors in Constructor
            }
            virtual void event_loop() final {
                int read4minus1 = 0;
                int write3plus1 = 0;
                while(stop_token.getUpdatedRef().test_and_set()){
                    if (handshake()) {
                        read_hook(read4minus1);
                        signaling_hook();
                        write_hook(write3plus1);
                    }
                    std::this_thread::yield();
                }
                stop_token.getAcknowledgeRef().clear();
            }
            protected:
            virtual void signaling_hook()=0;
            virtual bool handshake() = 0;
            virtual void handshake_ack() = 0;
            virtual void send_acknowledge() = 0;//3
            virtual void send_request() = 0;//1
            virtual bool receive_request() = 0;//2
            virtual bool receive_acknowledge() = 0;//4
            virtual unsigned char read_byte()=0;
            virtual void write_byte(unsigned char)=0;
            std::tuple<Objects...> objects{};
            DescriptorHelper<std::tuple_size<std::tuple<Objects...>>::value> descriptors{};
            bool mcu_updated = false;//mcu_write,fpga_read bit 7
            bool fpga_acknowledge = false;//mcu_write,fpga_read bit 6
            bool fpga_updated = false;//mcu_read,fpga_write bit 7
            bool mcu_acknowledge = false;//mcu_read,fpga_write bit 6
            private:
            bool receive_lock = false;
            bool send_lock = false;
            void read_hook(int& read4minus1){
                unsigned char data = read_byte();
                read_bits(static_cast<unsigned long>(data));
                if (!receive_lock){
                    if (receive_request()){
                        std::bitset<8> obj_id = static_cast<unsigned long>(data);
                        obj_id = (obj_id << 2) >> 2;
                        //check for "10111111"==idle==63
                        if (obj_id!=((idleState()<<2)>>2)){
                            auto id = static_cast<unsigned char>(obj_id.to_ulong());
                            for (std::size_t j=0;j<descriptors.size();j++){
                                if (descriptors[j].synced==true && descriptors[j].id==id){
                                    receive_lock=true;
                                    descriptors[j].readLock=true;
                                    readDestination = id;
                                    //std::cout<<typeid(*this).name()<<" starting ReadDestination "<<readDestination<<std::endl;
                                    readDestinationPos = 0;
                                    send_acknowledge();//DANGER: change writted state has to be after read_bits
                                }
                            }
                        }
                        //else {
                            //std::cout<<typeid(*this).name();
                            //std::cout<<".";
                        //}
                    }
                } else {
                    read(data);
                    if (read4minus1<3){
                        read4minus1++;
                    } else if (read4minus1==3){
                        auto read3bytes = read_flush();
                        if (readDestinationPos==descriptors[readDestination].obj_size){
                            descriptors[readDestination].readLock=false;
                            receive_lock=false;
                            //giving a read confirmation would require bidirectionalcontroller
                            descriptors[readDestination].rx_counter++;//DEBUG
                        } else {
                            for(std::size_t i=0;i<3;i++){
                                reinterpret_cast<char*>(descriptors[readDestination].obj)[readDestinationPos++]=read3bytes[i];
                            }
                        }
                        read4minus1 = 0;
                    }
                }
            }
            void write_hook(int& write3plus1){
                if (!send_lock){
                    if (receive_acknowledge()){
                        send_lock = true;
                        descriptors[writeOrigin].tx_counter++;//DEBUG
                    } else {
                        bool gotOne = false;
                        for (std::size_t i=0;i<descriptors.size()&& !gotOne;i++){
                            if (!descriptors[i].readLock && !descriptors[i].synced){
                                send_request();
                                writeOrigin=descriptors[i].id;
                                writeOriginPos=0;
                                std::bitset<8> id;
                                write_bits(id);
                                std::bitset<8> obj_id = static_cast<unsigned long>(writeOrigin);//DANGER: overflow check
                                id = id ^ obj_id;
                                //std::cout<<typeid(*this).name()<<" sending WriteOrigin "<<writeOrigin<<std::endl;
                                write_byte(static_cast<unsigned char>(id.to_ulong()));
                                handshake_ack();
                                gotOne=true;
                            }
                        }
                        //read in handshake -> set wire to valid state
                        if (!gotOne){
                            auto id = idleState();//10111111
                            write_bits(id);
                            id.set(7,1);//override write_bits
                            //std::cout<<typeid(*this).name();
                            //std::cout<<"!";
                            write_byte(static_cast<unsigned char>(id.to_ulong()));
                            handshake_ack();
                        }
                    }
                }
                if (send_lock) {
                    if (write3plus1<3){
                        unsigned char data;
                        data = reinterpret_cast<char*>(descriptors[writeOrigin].obj)[writeOriginPos++];
                        write(data);
                        handshake_ack();
                        write3plus1++;
                    } else if (write3plus1==3){
                        if (writeOriginPos==descriptors[writeOrigin].obj_size){
                            descriptors[writeOrigin].synced=true;
                            send_lock=false;
                            writeOriginPos=0;
                            //std::cout<<typeid(*this).name();
                            //std::cout<<"$";
                        }
                        write(63);//'?' empty write
                        handshake_ack();
                        write3plus1=0;
                    }
                }
            }
            unsigned int writeCount = 0;//write3plus1
            std::size_t writeOrigin = 0;
            std::size_t writeOriginPos = 0;
            unsigned int readCount = 0;//read4minus1
            std::size_t readDestination = 0;
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
                write_byte(static_cast<unsigned char>(out.to_ulong()));
            }
            bool read(unsigned char r){
                std::bitset<24> temp{ static_cast<unsigned long>(r)};
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
            virtual void read_bits(std::bitset<8> temp) = 0;
            static void read_shift(decltype(readAssembly)& readAssembly,decltype(readCount)& readCount, std::bitset<24>& temp){
                temp = temp<<(4-0)*4+2;//split off 1st 2bit
                temp = temp>>(readCount*3)*2;//shift
                readAssembly = readAssembly ^ temp;//overlay
            }
        };
        template<typename... Objects> class SerialFPGA : public virtual Serial<Objects...> {
            public:
            //SerialFPGA() : Serial<Objects...>() {}
            private:
            virtual void read_bits(std::bitset<8> temp) final {
                Serial<Objects...>::mcu_updated=temp[7];
                Serial<Objects...>::fpga_acknowledge=temp[6];
                Serial<Objects...>::mcu_acknowledge=false;
            }
            virtual void write_bits(std::bitset<8>& out) final {
                if (Serial<Objects...>::fpga_updated)
                    out.set(7,1);
                else
                    out.set(7,0);
                if (Serial<Objects...>::mcu_acknowledge)
                    out.set(6,1);
                else
                    out.set(6,0);
            }
            virtual void send_acknowledge() final {
                if (Serial<Objects...>::mcu_updated){
                    Serial<Objects...>::mcu_acknowledge=true;
                }
            }
            virtual void send_request() final {
                Serial<Objects...>::fpga_updated=true;
            }
            virtual bool receive_acknowledge() final {
                if (Serial<Objects...>::fpga_acknowledge){
                    Serial<Objects...>::fpga_updated=false;
                    return true;
                }
                return false;
            }
            virtual bool receive_request() final {
                return Serial<Objects...>::mcu_updated;
            }
        };
        template<typename... Objects> class SerialMCU : public virtual Serial<Objects...> {
            public:
            //SerialMCU() : Serial<Objects...>() {}
            private:
            virtual void read_bits(std::bitset<8> temp) final {
                Serial<Objects...>::fpga_updated=temp[7];
                Serial<Objects...>::mcu_acknowledge=temp[6];
                Serial<Objects...>::fpga_acknowledge=false;
            }
            virtual void write_bits(std::bitset<8>& out) final {
                if (Serial<Objects...>::mcu_updated)
                    out.set(7,1);
                else
                    out.set(7,0);
                if (Serial<Objects...>::fpga_acknowledge)
                    out.set(6,1);
                else
                    out.set(6,0);
            }
            virtual void send_acknowledge() final {
                if (Serial<Objects...>::fpga_updated){
                    Serial<Objects...>::fpga_acknowledge=true;
                }
            }
            virtual void send_request() final {
                Serial<Objects...>::mcu_updated=true;
            }
            virtual bool receive_acknowledge() final {
                if (Serial<Objects...>::mcu_acknowledge){
                    Serial<Objects...>::mcu_updated=false;
                    return true;
                }
                return false;
            }
            virtual bool receive_request() final {
                return Serial<Objects...>::fpga_updated;
            }
        };
    }
}