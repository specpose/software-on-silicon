#include "software-on-silicon/error.hpp"
#include "software-on-silicon/EventLoop.hpp"
#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <chrono>

namespace SOSFloat {
using SAMPLE_SIZE = float;
using MEMORY_CONTROLLER=std::array<SOS::MemoryView::Contiguous<SAMPLE_SIZE>*,10000>;
using READ_BUFFER=std::vector<SOS::MemoryView::ARAChannel<SOSFloat::SAMPLE_SIZE>>;

using namespace SOS::MemoryView;

class ReaderImpl : public SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>,
                    private SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    ReaderImpl(bus_type& blockerbus,SOS::MemoryView::ReaderBus<READ_BUFFER>& outside):
    SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>(blockerbus, outside),
    SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER>(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables))
    {
        //multiple inheritance: not ambiguous
        _thread = SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>::start(this);
    }
    ~ReaderImpl(){
        stop_requested = true;
        _thread.join();
    }
    void event_loop() final {
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
    bool stop_requested = false;//REMOVE: impl has to be in a valid state without stopping threads
    std::thread _thread;
};
class WriteTaskImpl : public SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl(const std::size_t& vst_numInputs) : SOS::Behavior::WriteTask<MEMORY_CONTROLLER>{} {
        _blocker.signal.getNotifyRef().clear();
        for(auto& entry : memorycontroller)
            entry = new SOS::MemoryView::Contiguous<typename std::remove_pointer<MEMORY_CONTROLLER::value_type>::type::value_type>(5);
        std::get<0>(_blocker.cables).getBKStartRef() = memorycontroller.begin();
        std::get<0>(_blocker.cables).getBKEndRef() = memorycontroller.end();
        _blocker.signal.getNotifyRef().test_and_set();
    }
    ~WriteTaskImpl(){
        _blocker.signal.getNotifyRef().clear();
        for(auto& entry : memorycontroller)
            if (entry)
                delete entry;
        _blocker.signal.getNotifyRef().test_and_set();
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
        SOS::MemoryView::ReaderBus<READ_BUFFER>& passThruHostMem,
        const std::size_t& vst_numInputs
        ) :
        WriteTaskImpl(vst_numInputs),
        PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>(_blocker,passThruHostMem)
        {
            //multiple inheritance: starts PassthruThread, not ReaderImpl
            _thread = PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
        };
    virtual ~WritePriorityImpl(){
        stop_requested = true;
        _thread.join();
    };
    //multiple inheritance: Overriding PassThru not ReaderImpl
    void event_loop(){
        int counter = 0;
        bool blink = true;
        while(!stop_requested){
        const auto start = high_resolution_clock::now();
        std::remove_pointer<MEMORY_CONTROLLER::value_type>::type data = SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5);
        if (blink){
            //auto entry = new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5);
            (data)[0]=0.0;//HACK: hard-coded channel 5
            (data)[1]=0.0;//HACK: hard-coded channel 5
            (data)[2]=0.0;//HACK: hard-coded channel 5
            (data)[3]=0.0;//HACK: hard-coded channel 5
            (data)[4]=1.0;//HACK: hard-coded channel 5
            //data=entry;
        }else{
            //auto entry = new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5);
            (data)[0]=0.0;//HACK: hard-coded channel 5
            (data)[1]=0.0;//HACK: hard-coded channel 5
            (data)[2]=0.0;//HACK: hard-coded channel 5
            (data)[3]=0.0;//HACK: hard-coded channel 5
            (data)[4]=0.0;//HACK: hard-coded channel 5
            //data=entry;
        }
        write(&data);
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
    bool stop_requested = false;//REMOVE: impl has to be in a valid state without stopping threads
    std::thread _thread;
};
}