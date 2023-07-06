#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/error.hpp"
#include <iostream>
#include <chrono>

using namespace SOS::MemoryView;

class ReaderImpl : public SOS::Behavior::Reader, private SOS::Behavior::ReadTask {
    public:
    ReaderImpl(bus_type& outside, BlockerBus& blockerbus) :
    SOS::Behavior::Reader(outside.signal),
    SOS::Behavior::ReadTask(std::get<0>(outside.const_cables),std::get<0>(outside.cables),blockerbus)
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
template<typename BufferType> class WriteTask : public SOS::Behavior::MemoryControllerWrite<BufferType> {
    public:
    WriteTask() :
     SOS::Behavior::MemoryControllerWrite<BufferType>{} {
        std::get<0>(_blocker.cables).getBKPosRef().store(std::get<0>(_blocker.const_cables).getBKStartRef());
        std::get<0>(_blocker.cables).getBKReaderPosRef().store(std::get<0>(_blocker.const_cables).getBKEndRef());
        this->memorycontroller.fill('-');
    }
    protected:
    void write(const char character) {
        auto pos = std::get<0>(_blocker.cables).getBKPosRef().load();
        if (pos!=std::get<0>(_blocker.const_cables).getBKEndRef()) {
            *(pos++)=character;
            std::get<0>(_blocker.cables).getBKPosRef().store(pos);
        } else {
            throw SFA::util::logic_error("Writer Buffer full",__FILE__,__func__);
        }
    }
    SOS::MemoryView::BlockerBus _blocker = SOS::MemoryView::BlockerBus(this->memorycontroller.begin(),this->memorycontroller.end());
};
using namespace std::chrono;
class WritePriorityImpl : private WriteTask<std::array<char,10000>>, private SOS::Behavior::WritePriority<ReaderImpl>  {
    public:
    WritePriorityImpl(
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::bus_type& writer,
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::subcontroller_type::bus_type& passThruHostMem
        ) :
        WriteTask{},
        SOS::Behavior::WritePriority<ReaderImpl>(passThruHostMem,_blocker) {
            
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
    auto hostmemory = std::array<char,1000>{};
    auto readerBus = ReaderBus(hostmemory.begin(),hostmemory.end());
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