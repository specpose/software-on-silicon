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
        static std::bitset<8> shutdownState() {//constexpr
            std::bitset<8> id;
            id.set(7,1);//updated==true
            id.set(6,0);//acknowledge==false
            id.set(0,0);
            //set 6bit data to "111110"
            for(std::size_t i = 1; i <= id.size()-1-2; i++){
                id.set(i,1);
            }
            return id;//-> "10111110"
        }
        struct DMADescriptor {
            DMADescriptor(){}//DANGER
            DMADescriptor(unsigned char id, void* obj, std::size_t obj_size) : id(id),obj(obj),obj_size(obj_size){
                if (obj_size%3!=0)
                    throw SFA::util::logic_error("Invalid DMAObject size",__FILE__,__func__);
                //check for "10111111" => >62
                const auto idleId = static_cast<unsigned long>(((idleState()<<2)>>2).to_ulong());
                if (id==idleId)
                    throw SFA::util::logic_error("DMADescriptor id is reserved for the serial line idle state",__FILE__,__func__);
                const auto shutdownId = static_cast<unsigned long>(((shutdownState()<<2)>>2).to_ulong());
                if (id==shutdownId)
                    throw SFA::util::logic_error("DMADescriptor id is reserved for the com_shutdown request on idle",__FILE__,__func__);
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
    }
    namespace MemoryView{
        struct DestinationAndOrigin : private SOS::MemoryView::TaskCable<std::size_t,2>{
            DestinationAndOrigin() : SOS::MemoryView::TaskCable<std::size_t,2>{0,0} {}
            auto& getReadDestinationRef(){return std::get<0>(*this);}
            auto& getWriteOriginRef(){return std::get<1>(*this);}
        };
        template<typename... Objects> struct SerialProcessNotifier : public SOS::MemoryView::BusShaker {
            using cables_type = std::tuple< DestinationAndOrigin >;
            SerialProcessNotifier() {
                std::apply(descriptors,objects);//ALWAYS: Initialize Descriptors in Constructor
            }
            cables_type cables;
            std::tuple<Objects...> objects{};
            SOS::Protocol::DescriptorHelper<std::tuple_size<std::tuple<Objects...>>::value> descriptors{};
            //bool com_shutdown = false;
        };
    }
    namespace Behavior {
        class SerialProcessing {
            public:
            SerialProcessing() {}
            void event_loop() {
                while(is_running()){
                    if (transfered()){
                        write_notify_hook();
                    }
                    if (received()){
                        read_notify_hook();
                    }
                    std::this_thread::yield();
                }
                finished();
            }
            protected:
            virtual bool is_running()=0;
            virtual void finished()=0;
            virtual bool received()=0;
            virtual bool transfered()=0;
            virtual void write_notify_hook()=0;
            virtual void read_notify_hook()=0;
        };
    }
    namespace Protocol {
        template<typename... Objects> class Serial {//write: 3 bytes in, 4 bytes out; read: 4 bytes in, 3 bytes out
            public:
            Serial() {}
            virtual void event_loop() {//final
                int read4minus1 = 0;
                int write3plus1 = 0;
                while(is_running()){
                    if (handshake()) {
                        read_hook(read4minus1);
                        write_hook(write3plus1);
                        handshake_ack();
                    }
                    std::this_thread::yield();
                }
                loop_shutdown = true;
                if (!com_shutdown){
                while ((!sent_com_shutdown && received_com_shutdown) ||
                        (sent_com_shutdown && !received_com_shutdown) ) {
                    if (handshake()){
                        read_hook(read4minus1);
                        write_hook(write3plus1);
                        handshake_ack();
                    }
                    std::this_thread::yield();
                }
                }
                finished();
                //std::cout<<typeid(*this).name()<<" shutdown"<<std::endl;
            }
            protected:
            virtual bool is_running() = 0;
            virtual void finished() = 0;
            virtual void request_stop() = 0;
            virtual bool handshake() = 0;
            virtual void handshake_ack() = 0;
            virtual void send_acknowledge() = 0;//3
            virtual void send_request() = 0;//1
            virtual bool receive_request() = 0;//2
            virtual bool receive_acknowledge() = 0;//4
            virtual unsigned char read_byte()=0;
            virtual void write_byte(unsigned char)=0;
            virtual void com_power_action()=0;
            virtual void com_shutdown_action()=0;
            void full_sync(){com_shutdown=false;};
            bool mcu_updated = false;//mcu_write,fpga_read bit 7
            bool fpga_acknowledge = false;//mcu_write,fpga_read bit 6
            bool fpga_updated = false;//mcu_read,fpga_write bit 7
            bool mcu_acknowledge = false;//mcu_read,fpga_write bit 6
            virtual constexpr typename SOS::MemoryView::SerialProcessNotifier<Objects...>& foreign() = 0;
            private:
            bool loop_shutdown = false;
            bool com_shutdown = false;
            bool received_com_shutdown = false;
            bool sent_com_shutdown = false;
            //ALIAS of Variables
            constexpr auto& readDestination() {
                return std::get<0>(foreign().cables).getReadDestinationRef();
            }
            constexpr auto& writeOrigin() {
                return std::get<0>(foreign().cables).getWriteOriginRef();
            }
            bool receive_lock = false;
            bool send_lock = false;
            void read_hook(int& read4minus1){
                unsigned char data = read_byte();
                read_bits(static_cast<unsigned long>(data));
                if (!receive_lock){
                    if (receive_request()){
                        std::bitset<8> obj_id = static_cast<unsigned long>(data);
                        obj_id = (obj_id << 2) >> 2;
                        if (obj_id==((idleState()<<2)>>2)){//check for "10111111"==idle==63
                            if (com_shutdown)
                                com_power_action();
                        } else if (obj_id==((shutdownState()<<2)>>2)) {//check for "10111110"==shutdown==63
                            com_shutdown = true;//incoming
                            //request_child_stop();
                            //std::cout<<typeid(*this).name();
                            //std::cout<<"O";
                        } else {
                            auto id = static_cast<unsigned char>(obj_id.to_ulong());
                            for (std::size_t j=0;j<foreign().descriptors.size();j++){
                                if (foreign().descriptors[j].synced==true && foreign().descriptors[j].id==id){
                                    receive_lock=true;
                                    foreign().descriptors[j].readLock=true;
                                    readDestination().store(id);
                                    //std::cout<<typeid(*this).name()<<" starting ReadDestination "<<readDestination()<<std::endl;
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
                        if (readDestinationPos==foreign().descriptors[readDestination().load()].obj_size){
                            foreign().descriptors[readDestination().load()].readLock=false;
                            receive_lock=false;
                            foreign().signal.getUpdatedRef().clear();
                            //giving a read confirmation would require bidirectionalcontroller
                            foreign().descriptors[readDestination().load()].rx_counter++;//DEBUG
                        } else {
                            for(std::size_t i=0;i<3;i++){
                                reinterpret_cast<char*>(foreign().descriptors[readDestination().load()].obj)[readDestinationPos++]=read3bytes[i];
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
                        foreign().signal.getAcknowledgeRef().clear();//Used as separate signals, not a handshake
                        foreign().descriptors[writeOrigin().load()].tx_counter++;//DEBUG
                    } else {
                        bool gotOne = false;
                        for (std::size_t i=0;i<foreign().descriptors.size()&& !gotOne;i++){
                            if (foreign().descriptors[i].readLock && !foreign().descriptors[i].synced)
                                throw SFA::util::logic_error("DMAObject has entered an illegal sync state.",__FILE__,__func__);
                            if (!foreign().descriptors[i].synced){
                                send_request();
                                writeOrigin().store(foreign().descriptors[i].id);
                                writeOriginPos=0;
                                std::bitset<8> id;
                                write_bits(id);
                                std::bitset<8> obj_id = static_cast<unsigned long>(writeOrigin().load());//DANGER: overflow check
                                id = id ^ obj_id;
                                //std::cout<<typeid(*this).name()<<" sending WriteOrigin "<<writeOrigin()<<std::endl;
                                write_byte(static_cast<unsigned char>(id.to_ulong()));
                                gotOne=true;
                            }
                        }
                        //read in handshake -> set wire to valid state
                        if (!gotOne){
                            if (loop_shutdown) {//Case of unsynced objects can be ignored from above
                                auto id = shutdownState();//10111110
                                write_bits(id);
                                id.set(7,1);//override write_bits
                                std::cout<<typeid(*this).name();
                                std::cout<<"X";
                                write_byte(static_cast<unsigned char>(id.to_ulong()));
                                sent_com_shutdown = true;
                            } else {
                                auto id = idleState();//10111111
                                write_bits(id);
                                id.set(7,1);//override write_bits
                                //std::cout<<typeid(*this).name();
                                //std::cout<<"!";
                                write_byte(static_cast<unsigned char>(id.to_ulong()));
                                if (com_shutdown){
                                    received_com_shutdown = true;
                                    com_shutdown_action();
                                }
                            }
                        }
                    }
                }
                if (send_lock) {
                    if (write3plus1<3){
                        unsigned char data;
                        data = reinterpret_cast<char*>(foreign().descriptors[writeOrigin().load()].obj)[writeOriginPos++];
                        write(data);
                        write3plus1++;
                    } else {//write3plus1==3
                        if (writeOriginPos==foreign().descriptors[writeOrigin().load()].obj_size){
                            foreign().descriptors[writeOrigin().load()].synced=true;
                            send_lock=false;
                            writeOriginPos=0;
                            //std::cout<<typeid(*this).name();
                            //std::cout<<"$";
                        }
                        write(63);//'?' empty write
                        write3plus1=0;
                    }
                }
            }
            unsigned int writeCount = 0;//write3plus1
            std::size_t writeOriginPos = 0;
            unsigned int readCount = 0;//read4minus1
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
                    case 1://recover last 1 2bit
                    case 2://recover last 2 2bit; call 3
                        out = write_assemble(writeAssembly, writeCount,w);
                        write_bits(out);
                        out = write_recover(writeAssembly, writeCount, out);
                        writeCount++;
                    break;
                    case 3://recover 3 2bit from call 3 only
                        write_bits(out);
                        out = write_recover(writeAssembly, writeCount, out);;
                        writeCount=0;
                    break;
                }
                write_byte(static_cast<unsigned char>(out.to_ulong()));
            }
            bool read(unsigned char r){
                std::bitset<24> temp{ static_cast<unsigned long>(r)};
                read_shift(readAssembly, readCount, temp);
                switch (readCount) {
                    case 0:
                    case 1:
                    case 2:
                    readCount++;
                    return true;
                    case 3:
                    readCount=0;
                }
                return false;
            }
            std::array<unsigned char,3> read_flush(){
                std::array<unsigned char, 3> result;// = *reinterpret_cast<std::array<unsigned char, 3>*>(&(readAssembly[0]));//wrong endianness
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
        template<typename... Objects> class SerialFPGA : protected virtual Serial<Objects...> {
            public:
            SerialFPGA() {}
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
        template<typename... Objects> class SerialMCU : protected virtual Serial<Objects...> {
            public:
            SerialMCU() {}
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
