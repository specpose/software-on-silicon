#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/simulation_helpers.hpp"

#include "Sample.cpp"
#define SAMPLE_TYPE char
#define NUM_CHANNELS 1
using RING_BUFFER=std::array<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>,334>;//INTERLEAVED
using MEMORY_CONTROLLER=std::array<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>,10000>;//INTERLEAVED
using BLOCK=std::array<MEMORY_CONTROLLER::value_type,1000>;

//main branch: Copy Start from MemoryController.cpp
class ReadTaskImpl : private virtual SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) 
    {}
    protected:
    void read(){
        auto current = _size.getReadBufferStartRef();
        const auto end = _size.getReadBufferAfterLastRef();
        const auto readOffset = _offset.getReadOffsetRef().load();
        if (readOffset<0)
            SFA::util::runtime_error(SFA::util::error_code::NegativeReadoffsetSupplied,__FILE__,__func__);
        if (std::distance(_memorycontroller_size.getBKStartRef(),_memorycontroller_size.getBKEndRef())
        <(std::distance(current,end)+readOffset))
            SFA::util::runtime_error(SFA::util::error_code::ReadindexOutOfBounds,__FILE__,__func__);
        auto readerPos = _memorycontroller_size.getBKStartRef()+readOffset;
        while (current!=end){
            if (!wait()) {
                *current = *readerPos;
                readerPos++;
                ++current;
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
    ReadTaskImpl(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables)),
    SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables))
    {
        //multiple inheritance: not ambiguous
        //_thread = SOS::Behavior::Reader<MEMORY_CONTROLLER,READ_SIZE>::start(this);
        _thread = start(this);
    }
    ~ReaderImpl(){
        destroy(_thread);
    }
//main branch: Copy Start from RingBuffer.cpp
    virtual void event_loop() final {
    while(Loop::is_running()){
        if (!_intrinsic.getUpdatedRef().test_and_set()){//random access call, FIFO
            //                        std::cout << "S";
            read();//FIFO whole buffer with intermittent waits when write
            //                        std::cout << "F";
            _intrinsic.getAcknowledgeRef().clear();
        }
    }
    Loop::finished();
    }
//main branch: Copy End from RingBuffer.cpp
    virtual void restart() final { _thread = SOS::Behavior::Stoppable::start(this); }
    private:
    virtual void read() final {
            ReadTaskImpl::read();
    };
    std::thread _thread;
};
class WriteTaskImpl : protected SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<MEMORY_CONTROLLER>() {
        this->memorycontroller.fill(MEMORY_CONTROLLER::value_type{'-'});
    }
    ~WriteTaskImpl(){}
    protected:
    virtual void write(RING_BUFFER::value_type& character) final {
        _blocker.signal.getWritingRef().clear();
        SOS::Behavior::WriteTask<MEMORY_CONTROLLER>::write(character);
        _blocker.signal.getWritingRef().test_and_set();
    }
};
class RingBufferTaskImpl : protected SOS::Behavior::RingBufferTask<RING_BUFFER> {
    public:
    using SOS::Behavior::RingBufferTask<RING_BUFFER>::RingBufferTask;
    protected:
    virtual void rb_write(RING_BUFFER::value_type& character) = 0;
    virtual void write(RING_BUFFER::value_type& character) final {};
};
//multiple inheritance: destruction order
class TransferRingToMemory : protected WriteTaskImpl, protected RingBufferTaskImpl {
    public:
    TransferRingToMemory(
        SOS::Behavior::RingBufferTask<RING_BUFFER>::cable_type& indices,
        SOS::Behavior::RingBufferTask<RING_BUFFER>::const_cable_type& bounds
        ) : WriteTaskImpl{}, RingBufferTaskImpl(indices, bounds) {}
    protected:
//main branch: Copy Start from RingBuffer.cpp
    virtual void read_loop() final {
        auto threadcurrent = _item.getThreadCurrentRef().load();
        auto current = _item.getCurrentRef().load();
        bool stop = false;
        while(!stop){//if: possible less writes than reads
            ++threadcurrent;
            if (threadcurrent==_bounds.getWriterEndRef())
                threadcurrent=_bounds.getWriterStartRef();
            if (threadcurrent!=current) {
                rb_write(*threadcurrent);
                _item.getThreadCurrentRef().store(threadcurrent);
            } else {
                stop = true;
            }
        }
    }
//main branch: Copy End from RingBuffer.cpp
    private:
    virtual void rb_write(RING_BUFFER::value_type& character) final {
        WriteTaskImpl::write(character);
    }
};
//multiple inheritance: destruction order
class RingBufferImpl : public SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>, public TransferRingToMemory {
    public:
    //multiple inheritance: construction order
    RingBufferImpl(SOS::MemoryView::RingBufferBus<RING_BUFFER>& rB,SOS::MemoryView::ReaderBus<BLOCK>& rd) :
    TransferRingToMemory(std::get<0>(rB.cables),std::get<0>(rB.const_cables)),
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
        while(is_running()){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                this->read_loop();
            }
            std::this_thread::yield();
        }
        finished();
    }
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};
