#include "software-on-silicon/error.hpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/memorycontroller_helpers.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include <chrono>

typedef char SAMPLE_SIZE;
#define STORAGE_SIZE 10000
#define MEMORY_CONTROLLER std::array<std::array<SAMPLE_SIZE,1>,STORAGE_SIZE>
#define READ_SIZE 1000
//#define READ_BUFFER std::array<std::array<SAMPLE_SIZE,1>,READ_SIZE>
namespace SOS {
    namespace MemoryView {
        template<> struct reader_traits<MEMORY_CONTROLLER> : public SFA::DeductionGuide<std::array<MEMORY_CONTROLLER::value_type,READ_SIZE>> {};
    }
}

using namespace SOS::MemoryView;
class ReadTaskImpl : private virtual SOS::Behavior::ReadTask<MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) 
    {}
    protected:
    virtual void read() {
        auto current = _size.getReadBufferStartRef();
        const auto end = _size.getReadBufferAfterLastRef();
        const auto readOffset = _offset.getReadOffsetRef().load();
        if (readOffset<0)
            throw SFA::util::runtime_error("Negative read offset supplied",__FILE__,__func__);
        if (std::distance(_memorycontroller_size.getBKStartRef(),_memorycontroller_size.getBKEndRef())
        <(std::distance(current,end)+readOffset))
            throw SFA::util::runtime_error("Read index out of bounds",__FILE__,__func__);
        auto readerPos = _memorycontroller_size.getBKStartRef()+readOffset;
        while (current!=end){
            if (!wait()) {
                *current = *(readerPos++);
                ++current;
                wait_acknowledge();
            }
            std::this_thread::yield();
        }
    }
};
class ReaderImpl : public SOS::Behavior::Reader<MEMORY_CONTROLLER>,
                    private virtual ReadTaskImpl {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>& blockerbus):
    SOS::Behavior::Reader<MEMORY_CONTROLLER>(outside, blockerbus),
    ReadTaskImpl(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables)),
    SOS::Behavior::ReadTask<MEMORY_CONTROLLER>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables))
    {
        //multiple inheritance: not ambiguous
        //_thread = SOS::Behavior::Reader<MEMORY_CONTROLLER>::start(this);
        _thread = start(this);
    }
    ~ReaderImpl(){
        //_thread.join();
        _thread.detach();
    }
    private:
    virtual void read() final {
        if (is_running()) {
            ReadTaskImpl::read();
        } else {
            request_stop();
        }
    };
    std::thread _thread;
};
class WriteTaskImpl : protected SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<MEMORY_CONTROLLER>() {
        this->memorycontroller.fill(std::array<SAMPLE_SIZE,1>{'-'});
    }
    protected:
    virtual void write(const MEMORY_CONTROLLER::value_type character) override {
        _blocker.signal.getUpdatedRef().clear();
        SOS::Behavior::WriteTask<MEMORY_CONTROLLER>::write(character);
        _blocker.signal.getUpdatedRef().test_and_set();
    }
};
using namespace std::chrono;

//multiple inheritance: destruction order
class WritePriorityImpl : public SOS::Behavior::PassthruAsyncController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>, private WriteTaskImpl {
    public:
    //multiple inheritance: construction order
    WritePriorityImpl(
        SOS::MemoryView::ReaderBus<SOS::MemoryView::reader_traits<MEMORY_CONTROLLER>::input_container_type>& passThruHostMem
        ) :
        WriteTaskImpl{},
        PassthruAsyncController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER> >(passThruHostMem,_blocker)
        {
            //multiple inheritance: starts PassthruAsync, not ReaderImpl
            //_thread = PassthruAsync<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
            _thread = start(this);
        };
    virtual ~WritePriorityImpl(){
        //_child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        //stop_token.getUpdatedRef().clear();
        //_thread.join();
        _thread.detach();
    };
    //multiple inheritance: Overriding PassThru not ReaderImpl
    void event_loop(){
        int counter = 0;
        bool blink = true;
        while(is_running()){
        const auto start = high_resolution_clock::now();
        MEMORY_CONTROLLER::value_type data;
        if (blink)
            data = std::array<SAMPLE_SIZE,1>{'*'};
        else
            data = std::array<SAMPLE_SIZE,1>{'_'};
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
        finished();
    }
    private:
    std::thread _thread;
};