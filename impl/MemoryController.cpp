#include "software-on-silicon/error.hpp"
#include "software-on-silicon/EventLoop.hpp"
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
        _thread = start(this);
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
    std::thread _thread;
};
class WriteTaskImpl : public SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl(const std::size_t vst_numInputs) : SOS::Behavior::WriteTask<MEMORY_CONTROLLER>(vst_numInputs) {
        std::get<0>(_blocker.cables).getBKStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef().store(memorycontroller.end());
        this->memorycontroller.fill(new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(vst_numInputs));
    }
};
using namespace std::chrono;

class WritePriorityImpl : public WriteTaskImpl, public PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>> {
    public:
    WritePriorityImpl(
        SOS::MemoryView::ReaderBus<READ_BUFFER>& passThruHostMem
        ) :
        WriteTaskImpl(std::get<1>(passThruHostMem.cables).size()),
        PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>(_blocker,passThruHostMem)
        {
            _thread = start(this);
        };
    virtual ~WritePriorityImpl(){
        stop_requested = true;
        _thread.join();
    };
    //Overriding PassThru not ReaderImpl!
    void event_loop(){
        int counter = 0;
        bool blink = true;
        while(!stop_requested){
        const auto start = high_resolution_clock::now();
        MEMORY_CONTROLLER::value_type data = new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5);
        if (blink){
            //auto entry = new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5);
            (*data)[4]=1.0;//HACK: hard-coded channel 5
            //data=entry;
        }else{
            //auto entry = new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5);
            (*data)[4]=0.0;//HACK: hard-coded channel 5
            //data=entry;
        }
        _blocker.signal.getNotifyRef().clear();
        write(data);
        //delete data;
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
}