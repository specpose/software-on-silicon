#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <iostream>
#include <chrono>

#include "Sample.cpp"
using MEMORY_CONTROLLER=std::array<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>,STORAGE_SIZE>;
using BLOCK=std::array<MEMORY_CONTROLLER::value_type,BLOCK_SIZE>;

class ReadTaskImpl : private virtual SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) 
    {}
    protected:
    virtual void read(){
        const auto start = _size.getReadBufferStartRef().load();
        auto current = start;
        const auto end = _size.getReadBufferAfterLastRef().load();
        auto readOffset = _offset.getReadOffsetRef().load();
        if (readOffset<0)
            SFA::util::runtime_error(SFA::util::error_code::NegativeReadoffsetSupplied,__FILE__,__func__);
        while (current!=end){
            if (!wait()) {
                const auto bk_start = _memorycontroller_size.getMCStartRef().load();
                const auto bk_end = _memorycontroller_size.getMCEndRef().load();
                if (std::distance(start, end) + readOffset > std::distance(bk_start, bk_end))
                    SFA::util::runtime_error(SFA::util::error_code::ReadindexOutOfBounds, __FILE__, __func__);
                *(current++) = *(bk_start+readOffset);
                readOffset++;
                wait_acknowledge();
            }
            std::this_thread::yield();
        }
    }
};

class ReaderImpl : public SOS::Behavior::Reader<BLOCK,MEMORY_CONTROLLER>,
                    private virtual ReadTaskImpl {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>& blockerbus):
    SOS::Behavior::Reader<BLOCK,MEMORY_CONTROLLER>(outside, blockerbus),
    ReadTaskImpl(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables)),
    SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER>(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables))
    {
        //multiple inheritance: not ambiguous
        //_thread = SOS::Behavior::Reader<MEMORY_CONTROLLER>::start(this);
        _thread = start(this);
    }
    ~ReaderImpl(){
        destroy(_thread);
    }
    private:
    virtual void read() final {
            ReadTaskImpl::read();
    };
    std::thread _thread;
};
class WriteTaskImpl : protected SOS::Behavior::NonBlockingWriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl() : SOS::Behavior::NonBlockingWriteTask<MEMORY_CONTROLLER>() {
        _blocker.signal.getWritingRef().clear();
        std::fill(std::begin(this->memorycontroller),std::end(this->memorycontroller),MEMORY_CONTROLLER::value_type{{0,0,0,0,0}});
        std::get<0>(_blocker.cables).getMCStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getMCEndRef().store(memorycontroller.end());
        _blocker.signal.getWritingRef().test_and_set();
    }
    ~WriteTaskImpl(){}
};
using namespace std::chrono;

//multiple inheritance: destruction order
class WritePriorityImpl : public SOS::Behavior::PassthruAsyncController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>, private WriteTaskImpl {
    public:
    //multiple inheritance: construction order
    WritePriorityImpl(
        SOS::MemoryView::ReaderBus<BLOCK>& passThruHostMem
        ) :
        WriteTaskImpl(),
        PassthruAsyncController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER> >(passThruHostMem,_blocker)
        {
            //multiple inheritance: starts PassthruAsync, not ReaderImpl
            //_thread = PassthruAsync<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
            _thread = start(this);
        };
    virtual ~WritePriorityImpl(){
        destroy(_thread);
    };
    //multiple inheritance: Overriding PassThru not ReaderImpl
    void event_loop(){
        const auto start = high_resolution_clock::now();
        MEMORY_CONTROLLER::value_type data;
        if (blink)
            data = MEMORY_CONTROLLER::value_type{{0,0,0,0,1}};
        else
            data = MEMORY_CONTROLLER::value_type{{0,0,0,0,0}};
        block(1);
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
    private:
    int counter = 0;
    bool blink = true;
    std::thread _thread;
};