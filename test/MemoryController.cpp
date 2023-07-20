#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <iostream>
#include <chrono>

#define MEMORY_CONTROLLER std::array<char,10000>
#define READ_BUFFER std::array<char,1000>

using namespace SOS::MemoryView;

class ReaderImpl : public SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>,
                    private SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    ReaderImpl(bus_type& blockerbus,SOS::MemoryView::ReaderBus<READ_BUFFER>& outside):
    SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>(blockerbus, outside),
    SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables))
    {
        _thread = start(this);
    }
    ~ReaderImpl(){
        stop_requested = true;
        _thread.join();
    }
    void event_loop() {
        while(!stop_requested){
            fifo_loop();
            std::this_thread::yield();
        }
    }
    void fifo_loop() {
        if (!_intrinsic.getUpdatedRef().test_and_set()){//random access call, FIFO
//                        std::cout << "S";
            SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER>::read();//FIFO whole buffer with intermittent waits when write
//                        std::cout << "F";
            _intrinsic.getAcknowledgeRef().clear();
        }
    }
    bool wait() final {
        if (!_blocked_signal.getNotifyRef().test_and_set()) {//intermittent wait when write
            _blocked_signal.getNotifyRef().clear();
            return true;
        } else {
            return false;
        }
    }
    private:
    std::thread _thread;
};
class WriteTaskImpl : public SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<MEMORY_CONTROLLER>() {
        this->memorycontroller.fill('-');
    }
};
using namespace std::chrono;

class WritePriority : protected WriteTaskImpl, public PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>> {
    public:
    WritePriority(
        SOS::MemoryView::ReaderBus<READ_BUFFER>& passThru
        ) : WriteTaskImpl{}, PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>(_blocker,passThru) {};
    virtual ~WritePriority(){};
};
class WritePriorityImpl : public WritePriority {
    public:
    WritePriorityImpl(
        SOS::MemoryView::ReaderBus<READ_BUFFER>& passThruHostMem
        ) :
        WritePriority{passThruHostMem} {
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
        MEMORY_CONTROLLER::value_type data;
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
    }
    private:
    bool stop_requested = false;
    std::thread _thread;
};

int main(){
    auto randomread = READ_BUFFER{};
    auto readerBus = ReaderBus<READ_BUFFER>(randomread.begin(),randomread.end());
    std::cout << "Reader reading 1000 times at tail of memory at rate 1/s..." << std::endl;
    std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
    auto controller = new WritePriorityImpl(readerBus);
    readerBus.setOffset(9000);//FIFO has to be called before each getUpdatedRef().clear()
    readerBus.signal.getUpdatedRef().clear();
    while (true){
        const auto start = high_resolution_clock::now();
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.setOffset(9000);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << *print++;
            std::cout << std::endl;
        }
        std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(seconds{1}));
    }
    if (controller!=nullptr)
        delete controller;
}