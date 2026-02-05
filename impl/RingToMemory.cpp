#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <chrono>

using namespace std::chrono;

#include "Sample.cpp"
#define SAMPLE_TYPE float
#define NUM_CHANNELS 1
#define MAX_BLINK 333
using RING_BUFFER=std::array<std::tuple<std::array<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>,MAX_BLINK>,std::size_t>,4>;//400//INTERLEAVED
using MEMORY_CONTROLLER=std::vector<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>>;//INTERLEAVED
using BLOCK=std::array<MEMORY_CONTROLLER::value_type,1000>;//ara_samplesPerChannel = 1000

//main branch: Copy Start from MemoryController.cpp
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
                const auto bk_start = _memorycontroller_size.getBKStartRef().load();
                const auto bk_end = _memorycontroller_size.getBKEndRef().load();
                if (std::distance(start,current) >= std::distance(bk_start,bk_end)-readOffset){
                    //SFA::util::runtime_error(SFA::util::error_code::ReadindexOutOfBounds, __FILE__, __func__);
                    *(current++) = BLOCK::value_type{{0}};
                } else {
                    *(current++) = *(bk_start+readOffset);
                }
                readOffset++;
                wait_acknowledge();
            }
            std::this_thread::yield();
        }
    }
};
//main branch: Copy End from MemoryController.cpp

class ReaderImpl : public SOS::Behavior::Reader<BLOCK,MEMORY_CONTROLLER>,
                    private virtual ReadTaskImpl {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>& blockerbus):
    SOS::Behavior::Reader<BLOCK,MEMORY_CONTROLLER>(outside, blockerbus),
    ReadTaskImpl(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables)),
    SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER>(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables))
    {
        //multiple inheritance: not ambiguous
        //_thread = SOS::Behavior::Reader<MEMORY_CONTROLLER,READ_SIZE>::start(this);
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
class WriteTaskImpl : protected SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<MEMORY_CONTROLLER>(),
    ara_sampleCount(0) {
        _blocker.signal.getWritingRef().clear();
        std::fill(std::begin(memorycontroller),std::end(memorycontroller),MEMORY_CONTROLLER::value_type{0});
        _blocker.signal.getWritingRef().test_and_set();
    }
    ~WriteTaskImpl(){}
    virtual void resize(MEMORY_CONTROLLER::difference_type newsize){
        _blocker.signal.getWritingRef().clear();
        memorycontroller.reserve(newsize);
        while(memorycontroller.size()<newsize){
            memorycontroller.push_back(MEMORY_CONTROLLER::value_type{0});
        }
        std::get<0>(_blocker.cables).getBKStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef().store(memorycontroller.end());
        ara_sampleCount = memorycontroller.size();
        _blocker.signal.getWritingRef().test_and_set();
    };
    MEMORY_CONTROLLER::difference_type ara_sampleCount;
    //not inherited: overload
    protected:
    virtual void write(RING_BUFFER::value_type& character) final {//takes absolutePosition out of Ringbuffer
        resize(std::get<1>(character)+std::size(std::get<0>(character)));//offset + length
        if (std::distance(std::get<0>(_blocker.cables).getBKStartRef().load(),std::get<0>(_blocker.cables).getBKEndRef().load())<
        std::get<1>(character)+std::size(std::get<0>(character)))
            SFA::util::runtime_error(SFA::util::error_code::WriterTriedToWriteBeyondMemorycontrollerBounds,__FILE__,__func__);
        writerPos = std::get<0>(_blocker.cables).getBKStartRef().load() + std::get<1>(character);
        for(std::size_t i=0;i<std::size(std::get<0>(character));i++){
            SOS::Behavior::WriteTask<MEMORY_CONTROLLER>::write(std::get<0>(character)[i]);
        }
    }
};
//multiple inheritance: destruction order
class RingBufferTaskImpl : protected SOS::Behavior::RingBufferTask<RING_BUFFER>, public WriteTaskImpl {
    public:
    RingBufferTaskImpl(
        SOS::Behavior::RingBufferTask<RING_BUFFER>::cable_type& indices,
        SOS::Behavior::RingBufferTask<RING_BUFFER>::const_cable_type& bounds
        ) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds), WriteTaskImpl{} {}
    private:
    //overrides RingBufferTask::transfer and remaps to WriteTaskImpl::write
    virtual void transfer(RING_BUFFER::value_type& character) final {
        WriteTaskImpl::write(character);
    }
};
//multiple inheritance: destruction order
class RingBufferImpl : public SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>, public RingBufferTaskImpl {
    public:
    //multiple inheritance: construction order
    RingBufferImpl(SOS::MemoryView::RingBufferBus<RING_BUFFER>& rB,SOS::MemoryView::ReaderBus<BLOCK>& rd) :
    RingBufferTaskImpl(std::get<0>(rB.cables),std::get<0>(rB.const_cables)),
    SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>(rB.signal,rd,_blocker)
    {
        //multiple inheritance: PassthruSimpleController, not ReaderImpl
        //_thread = SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
        _thread = start(this);
    }
    ~RingBufferImpl() final{
        destroy(_thread);
    }
    //multiple inheritance: Overriding RingBufferImpl, not ReaderImpl
    void event_loop(){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                this->read_loop();
            }
            std::this_thread::yield();
    }
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};