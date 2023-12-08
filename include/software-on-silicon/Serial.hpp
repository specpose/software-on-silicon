#include <bitset>
#include <cmath>
namespace SOS {
    namespace MemoryView {
        struct DestinationAndOrigin : private SOS::MemoryView::TaskCable<std::size_t,2>{
            DestinationAndOrigin() : SOS::MemoryView::TaskCable<std::size_t,2>{0,0} {}
            auto& getReadDestinationRef(){return std::get<0>(*this);}
            auto& getWriteOriginRef(){return std::get<1>(*this);}
        };
        struct SerialProcessNotifier : public SOS::MemoryView::BusShaker {
            using const_cables_type = std::tuple< DestinationAndOrigin >;
            const_cables_type const_cables;
        };
    }
    namespace Behavior {
    }
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
        template<unsigned long Bytes> constexpr unsigned long long maxUnsignedInt() {
            unsigned long long result = 0;
            for (int i=0;i<Bytes;i++){
                result+= 0xFF*std::pow(256,i);
            }
            return result;
        };
        template<unsigned long Bytes> constexpr unsigned long long bytearrayToUnsignedNumber(std::array<unsigned char,Bytes>& source){
            unsigned long long result = 0;
            for (int i=0;i<Bytes;i++){
                result+= static_cast<unsigned char>(source[i])*std::pow(256,i);
            }
            return result;
        };
        template<unsigned long Bytes, unsigned long Bits> constexpr void bitsetToBytearray(std::array<unsigned char,Bytes>& dest,std::bitset<Bits>& source){
            if (source.to_ullong() > maxUnsignedInt<Bytes>())
                throw SFA::util::logic_error("BitSet does not fit into char[]",__FILE__,__func__);
            for (int i=0;i<Bytes;i++)//bitset is little endian, first destination byte is bigend
                dest[i]=static_cast<unsigned char>(((source << (Bytes-(Bytes-i))*8) >> (Bytes-1)*8).to_ulong());;
        };
        template<unsigned long Bytes, unsigned long Bits> constexpr void bytearrayToBitset(std::bitset<Bits>& dest,std::array<unsigned char,Bytes>& source){
            std::bitset<Bits> allSet;
            allSet.set();
            if (maxUnsignedInt<Bytes>() > allSet.to_ullong())
                throw SFA::util::logic_error("Numeric value of BitSet does not fit into char[]",__FILE__,__func__);
            dest.reset();
            for (int i=0;i<Bytes;i++){
                std::bitset<Bits> digit = static_cast<unsigned int>(source[i])*std::pow(256,(Bytes-1)-i);//char[] is used as little endian, first read byte is bigend
                dest = dest ^ digit;
            }
        };
        template<typename ProcessingHook,typename... Objects> class Serial : public SOS::Behavior::Loop,
        public virtual SOS::Behavior::EventController<ProcessingHook> {//write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
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
                        write_hook(write3plus1);
                    }
                    std::this_thread::yield();
                }
                stop_token.getAcknowledgeRef().clear();
            }
            protected:
            //virtual void signaling_hook()=0;
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
            std::atomic<std::size_t>& readDestination = std::get<0>(SOS::Behavior::EventController<ProcessingHook>::_foreign.const_cables).getReadDestinationRef();
            std::atomic<std::size_t>& writeOrigin = std::get<0>(SOS::Behavior::EventController<ProcessingHook>::_foreign.const_cables).getWriteOriginRef();
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
                                    readDestination.store(id);
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
                        if (readDestinationPos==descriptors[readDestination.load()].obj_size){
                            descriptors[readDestination.load()].readLock=false;
                            receive_lock=false;
                            SOS::Behavior::EventController<ProcessingHook>::_foreign.signal.getUpdatedRef().clear();
                            //giving a read confirmation would require bidirectionalcontroller
                            descriptors[readDestination.load()].rx_counter++;//DEBUG
                        } else {
                            for(std::size_t i=0;i<3;i++){
                                reinterpret_cast<char*>(descriptors[readDestination.load()].obj)[readDestinationPos++]=read3bytes[i];
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
                        SOS::Behavior::EventController<ProcessingHook>::_foreign.signal.getAcknowledgeRef().clear();//Used as separate signals, not a handshake
                        descriptors[writeOrigin.load()].tx_counter++;//DEBUG
                    } else {
                        bool gotOne = false;
                        for (std::size_t i=0;i<descriptors.size()&& !gotOne;i++){
                            if (!descriptors[i].readLock && !descriptors[i].synced){
                                send_request();
                                writeOrigin.store(descriptors[i].id);
                                writeOriginPos=0;
                                std::bitset<8> id;
                                write_bits(id);
                                std::bitset<8> obj_id = static_cast<unsigned long>(writeOrigin.load());//DANGER: overflow check
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
                        data = reinterpret_cast<char*>(descriptors[writeOrigin.load()].obj)[writeOriginPos++];
                        write(data);
                        handshake_ack();
                        write3plus1++;
                    } else if (write3plus1==3){
                        if (writeOriginPos==descriptors[writeOrigin.load()].obj_size){
                            descriptors[writeOrigin.load()].synced=true;
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
            //std::size_t writeOrigin = 0;
            std::size_t writeOriginPos = 0;
            unsigned int readCount = 0;//read4minus1
            //std::size_t readDestination = 0;
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
                //result[0] = static_cast<unsigned char>((readAssembly >> 16).to_ulong());
                //result[1] = static_cast<unsigned char>(((readAssembly << 8)>> 16).to_ulong());
                //result[2] = static_cast<unsigned char>(((readAssembly << 16) >> 16).to_ulong());
                bitsetToBytearray(result,readAssembly);
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
        template<typename ProcessingHook, typename... Objects> class SerialFPGA : private virtual Serial<ProcessingHook, Objects...> {
            public:
            SerialFPGA() {}
            private:
            virtual void read_bits(std::bitset<8> temp) final {
                Serial<ProcessingHook, Objects...>::mcu_updated=temp[7];
                Serial<ProcessingHook, Objects...>::fpga_acknowledge=temp[6];
                Serial<ProcessingHook, Objects...>::mcu_acknowledge=false;
            }
            virtual void write_bits(std::bitset<8>& out) final {
                if (Serial<ProcessingHook, Objects...>::fpga_updated)
                    out.set(7,1);
                else
                    out.set(7,0);
                if (Serial<ProcessingHook, Objects...>::mcu_acknowledge)
                    out.set(6,1);
                else
                    out.set(6,0);
            }
            virtual void send_acknowledge() final {
                if (Serial<ProcessingHook, Objects...>::mcu_updated){
                    Serial<ProcessingHook, Objects...>::mcu_acknowledge=true;
                }
            }
            virtual void send_request() final {
                Serial<ProcessingHook, Objects...>::fpga_updated=true;
            }
            virtual bool receive_acknowledge() final {
                if (Serial<ProcessingHook, Objects...>::fpga_acknowledge){
                    Serial<ProcessingHook, Objects...>::fpga_updated=false;
                    return true;
                }
                return false;
            }
            virtual bool receive_request() final {
                return Serial<ProcessingHook, Objects...>::mcu_updated;
            }
        };
        template<typename ProcessingHook, typename... Objects> class SerialMCU : private virtual Serial<ProcessingHook, Objects...> {
            public:
            SerialMCU() {}
            private:
            virtual void read_bits(std::bitset<8> temp) final {
                Serial<ProcessingHook, Objects...>::fpga_updated=temp[7];
                Serial<ProcessingHook, Objects...>::mcu_acknowledge=temp[6];
                Serial<ProcessingHook, Objects...>::fpga_acknowledge=false;
            }
            virtual void write_bits(std::bitset<8>& out) final {
                if (Serial<ProcessingHook, Objects...>::mcu_updated)
                    out.set(7,1);
                else
                    out.set(7,0);
                if (Serial<ProcessingHook, Objects...>::fpga_acknowledge)
                    out.set(6,1);
                else
                    out.set(6,0);
            }
            virtual void send_acknowledge() final {
                if (Serial<ProcessingHook, Objects...>::fpga_updated){
                    Serial<ProcessingHook, Objects...>::fpga_acknowledge=true;
                }
            }
            virtual void send_request() final {
                Serial<ProcessingHook, Objects...>::mcu_updated=true;
            }
            virtual bool receive_acknowledge() final {
                if (Serial<ProcessingHook, Objects...>::mcu_acknowledge){
                    Serial<ProcessingHook, Objects...>::mcu_updated=false;
                    return true;
                }
                return false;
            }
            virtual bool receive_request() final {
                return Serial<ProcessingHook, Objects...>::fpga_updated;
            }
        };
    }
}