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
class WriteTask : private SOS::Behavior::MemoryControllerWrite {
    public:
    using _buffer_type = std::array<char,10000>;
    WriteTask(blocker_ct& posBlocker,blocker_buffer_size& bufferSize) :
     SOS::Behavior::MemoryControllerWrite(posBlocker,bufferSize) {
        resize(memorycontroller.begin(),memorycontroller.end());
        memorycontroller.fill('-');
    }
    protected:
    void write(const char character) {
        auto pos = _item.getBKPosRef().load();
        if (pos!=memorycontroller.end()) {
            *(pos++)=character;
            _item.getBKPosRef().store(pos);
        } else {
            throw SFA::util::logic_error("Writer Buffer full",__FILE__,__func__);
        }
    }
    void resize(_buffer_type::iterator start, _buffer_type::iterator end){
        _size.getBKStartRef()=start;
        _size.getBKEndRef()=end;
        _item.getBKPosRef().store(start);
        _item.getBKReaderPosRef().store(end);
    };
    private:
    _buffer_type memorycontroller = _buffer_type{};
};
using namespace std::chrono;
class WritePriorityImpl : private SOS::Behavior::WritePriority<ReaderImpl>, private WriteTask {
    public:
    WritePriorityImpl(
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::bus_type& writer,
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::subcontroller_type::bus_type& passThruHostMem
        ) : SOS::Behavior::WritePriority<ReaderImpl>(passThruHostMem), WriteTask(std::get<0>(_blocker.cables),std::get<1>(_blocker.cables)) {
            
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