#include "software-on-silicon/HostMemoryController.hpp"
#include "software-on-silicon/error.hpp"
#include <iostream>
#include <chrono>

#define MEMORY_CONTROLLER std::array<char,10000>
#define READ_BUFFER std::array<char,1000>

using namespace SOS::MemoryView;

struct ReaderBusImpl : public ReaderBus<READ_BUFFER> {
    using ReaderBus<READ_BUFFER>::ReaderBus;
};
struct BlockerBusImpl : public BlockerBus<MEMORY_CONTROLLER> {
    using BlockerBus<MEMORY_CONTROLLER>::BlockerBus;
};
class ReaderImpl : public SOS::Behavior::Reader<ReaderBusImpl>, private SOS::Behavior::ReadTask<ReaderBusImpl,BlockerBusImpl> {
    public:
    ReaderImpl(bus_type& outside, BlockerBusImpl& blockerbus) ://variadic: blockerbus can not be derived from Reader
    SOS::Behavior::Reader<ReaderBusImpl>(outside.signal),
    SOS::Behavior::ReadTask<ReaderBusImpl,BlockerBusImpl>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),blockerbus)
    {
        std::cout << "Reader reading 1000 times at tail of memory at rate 1/s..." << std::endl;
        _thread = start(this);
    }
    ~ReaderImpl(){
        stop_requested = true;
        _thread.join();
    }
    void event_loop(){
        while(!stop_requested){
            if (!_intrinsic.getUpdatedRef().test_and_set()){
                std::cout << "S";
                read();
                std::cout << "F";
                _intrinsic.getAcknowledgeRef().clear();
            }
        }
    }
    private:
    bool stop_requested = false;
    std::thread _thread;
};
class WriteTaskImpl : public SOS::Behavior::WriteTask<MEMORY_CONTROLLER,BlockerBusImpl> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<MEMORY_CONTROLLER,BlockerBusImpl>() {
        this->memorycontroller.fill('-');
    }
};
//RunLoop does not support more than one constructor arguments
//RunLoop does not forward passThru
template<typename ReaderType> class WritePriority : protected WriteTaskImpl, public SOS::Behavior::Loop {
    public:
    using subcontroller_type = ReaderType;
    using bus_type = typename SOS::Behavior::RunLoop<subcontroller_type>::bus_type;
    WritePriority(
        typename SOS::Behavior::RunLoop<subcontroller_type>::subcontroller_type::bus_type& passThru
        ) : WriteTaskImpl{}, _child(ReaderType{passThru,_blocker}) {};//calling variadic constructor?
    virtual ~WritePriority(){};
    void event_loop(){}
    private:
    ReaderType _child;
};
using namespace std::chrono;
class WritePriorityImpl : private WritePriority<ReaderImpl>  {
    public:
    WritePriorityImpl(
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::bus_type& writer,
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::subcontroller_type::bus_type& passThruHostMem
        ) :
        //variadic?
        WritePriority<ReaderImpl>{passThruHostMem} {
            std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
            _thread = start(this);
        };
    virtual ~WritePriorityImpl(){
        stop_requested = true;
        _thread.join();
    };
    void event_loop(){
        int counter = 0;
        bool blink = true;
        while(!stop_requested){
        const auto start = high_resolution_clock::now();
        char data;
        if (blink)
            data = '*';
        else
            data = '_';
        _blocker.signal.getNotifyRef().clear();
        write(data);
        _blocker.signal.getNotifyRef().test_and_set();
        counter++;
        if (blink && counter==333){
            blink = false;
            counter=0;
        } else if (!blink && counter==666) {
            blink = true;
            counter=0;
        }
        std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
        }
    };
    private:
    bool stop_requested = false;
    std::thread _thread;
};

using namespace std::chrono;

int main(){
    auto writerBus = BusNotifier{};
    auto hostmemory = READ_BUFFER{};
    //decltype(hostmemory)?
    auto readerBus = ReaderBusImpl(hostmemory.begin(),hostmemory.end());
    readerBus.setOffset(9000);
    auto controller = new WritePriorityImpl(writerBus,readerBus);
    readerBus.signal.getUpdatedRef().clear();
    while (true){
        const auto start = high_resolution_clock::now();
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.signal.getUpdatedRef().clear();
            auto print = hostmemory.begin();
            while (print!=hostmemory.end())
                std::cout << *print++;
            std::cout << std::endl;
        }
        std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(seconds{1}));
    }
    if (controller!=nullptr)
        delete controller;
}