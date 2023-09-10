#include "software-on-silicon/error.hpp"
#include "software-on-silicon/EventLoop.hpp"
#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
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
        //multiple inheritance: not ambiguous
        _thread = SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>::start(this);
    }
    ~ReaderImpl(){
        stop_token.getUpdatedRef().clear();
        _thread.join();
    }
    void event_loop() final {
        while(stop_token.getUpdatedRef().test_and_set()){
            fifo_loop();
            std::this_thread::yield();
        }
        stop_token.getAcknowledgeRef().clear();
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
    protected:
    virtual void write(const MEMORY_CONTROLLER::value_type character) override {
        _blocker.signal.getNotifyRef().clear();
        SOS::Behavior::WriteTask<MEMORY_CONTROLLER>::write(character);
        _blocker.signal.getNotifyRef().test_and_set();
    }
};
using namespace std::chrono;

//multiple inheritance: destruction order
class WritePriorityImpl : public PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>, public WriteTaskImpl {
    public:
    //multiple inheritance: construction order
    WritePriorityImpl(
        SOS::MemoryView::ReaderBus<READ_BUFFER>& passThruHostMem
        ) :
        WriteTaskImpl{},
        PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>(_blocker,passThruHostMem)
        {
            //multiple inheritance: starts PassthruThread, not ReaderImpl
            _thread = PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
        };
    virtual ~WritePriorityImpl(){
        _child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
    };
    //multiple inheritance: Overriding PassThru not ReaderImpl
    void event_loop(){
        int counter = 0;
        bool blink = true;
        while(stop_token.getUpdatedRef().test_and_set()){
        const auto start = high_resolution_clock::now();
        MEMORY_CONTROLLER::value_type data;
        if (blink)
            data = '*';
        else
            data = '_';
        write(data);
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
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    std::thread _thread;
};