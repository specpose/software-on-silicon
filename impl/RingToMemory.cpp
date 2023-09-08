#include "software-on-silicon/error.hpp"
#include "software-on-silicon/EventLoop.hpp"
#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "software-on-silicon/MemoryController.hpp"

using namespace SOS;

namespace SOSFloat {
using SAMPLE_SIZE = float;
using RING_BUFFER = std::array<std::tuple<SOS::MemoryView::Contiguous<SAMPLE_SIZE>**,unsigned int,unsigned int>,2>;//0:[maxSamplesPerProcess][vst_numInputs], 1: vst_processSamples, 2: ara_samplePosition
using MEMORY_CONTROLLER=std::vector<SOS::MemoryView::Contiguous<SAMPLE_SIZE>*>;
using READ_BUFFER=std::vector<SOS::MemoryView::ARAChannel<SOSFloat::SAMPLE_SIZE>>;

class ReadTaskImpl : public SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) :
    SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER>(Length, Offset, blockercable)
    {}
    protected:
    void read(){
        for (std::size_t channel=0;channel<_size.size();channel++){
        //readbuffer
        const auto start = _size[channel].getReadBufferStartRef().load();
        auto current = start;
        const auto end = _size[channel].getReadBufferAfterLastRef().load();
        //memorycontroller
        auto readOffset = _offset.getReadOffsetRef().load();
        if (readOffset<0)
            throw SFA::util::runtime_error("Negative read offset supplied",__FILE__,__func__);
        while (current!=end){
            if (!wait()) {
                auto readerStart = _memorycontroller_size.getBKStartRef().load();
                readerStart += readOffset;
                const auto readerEnd = _memorycontroller_size.getBKEndRef().load();
                //if the distance of the lval from its start is bigger than
                //the (the rval offset to rval end)
                if (std::distance(start,current)>=std::distance(readerStart,readerEnd)){
                    *current = 0.0;
                } else {
                    *current = (**(readerStart))[channel];
                    readOffset++;
                }
                ++current;
            }
        }
        }
    }
};
class ReaderImpl : public SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>,
                    private ReadTaskImpl {
    public:
    ReaderImpl(bus_type& blockerbus,SOS::MemoryView::ReaderBus<READ_BUFFER>& outside):
    SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>(blockerbus, outside),
    ReadTaskImpl(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables))
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
            ReadTaskImpl::read();//FIFO whole buffer with intermittent waits when write
            _intrinsic.getAcknowledgeRef().clear();
        }
    }
    virtual bool wait() final {
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
    WriteTaskImpl(const std::size_t& vst_numInputs) :
    SOS::Behavior::WriteTask<MEMORY_CONTROLLER>(),
    _vst_numInputs(vst_numInputs),
    ara_sampleCount(0) {}
    ~WriteTaskImpl(){
        clearMemoryController();
    }
    virtual void resize(MEMORY_CONTROLLER::difference_type newsize){
        memorycontroller.reserve(newsize);
        while(memorycontroller.size()<newsize){
            memorycontroller.push_back(new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(_vst_numInputs));
        }
        std::get<0>(_blocker.cables).getBKStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef().store(memorycontroller.end());
        ara_sampleCount = memorycontroller.size();
        for(auto& sample : memorycontroller)
            if (sample->size()!=_vst_numInputs)
                throw SFA::util::logic_error("Memorycontroller resize error",__FILE__,__func__);
    };
    //not inherited: overload
    void write(const RING_BUFFER::value_type character) {
        _blocker.signal.getNotifyRef().clear();
        resize(std::get<2>(character)+std::get<1>(character));//offset + length
        _blocker.signal.getNotifyRef().test_and_set();
        if (std::distance(std::get<0>(_blocker.cables).getBKStartRef().load(),std::get<0>(_blocker.cables).getBKEndRef().load())<
        std::get<2>(character)+std::get<1>(character))
            throw SFA::util::runtime_error("Writer tried to write beyond memorycontroller bounds",__FILE__,__func__);
        writerPos = std::get<0>(_blocker.cables).getBKStartRef().load() + std::get<2>(character);
        for(std::size_t i=0;i<std::get<1>(character);i++){
            _blocker.signal.getNotifyRef().clear();
            SOS::Behavior::WriteTask<MEMORY_CONTROLLER>::write((std::get<0>(character))[i]);
            _blocker.signal.getNotifyRef().test_and_set();
        }
    }
    //not inherited
    void clearMemoryController() {
        _blocker.signal.getNotifyRef().clear();
        ara_sampleCount = 0;
        for(std::size_t i = 0;i<memorycontroller.size();i++)
            if (memorycontroller[i]){
                delete memorycontroller[i];
                memorycontroller[i] = nullptr;
            }
            else {
                throw SFA::util::logic_error("MemoryController corrupted",__FILE__,__func__);
            }
        memorycontroller.clear();
        std::get<0>(_blocker.cables).getBKStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef().store(memorycontroller.end());
        _blocker.signal.getNotifyRef().test_and_set();
    }
    MEMORY_CONTROLLER::difference_type ara_sampleCount;
    private:
    const std::size_t& _vst_numInputs;
};
//multiple inheritance: destruction order
class TransferRingToMemory : public WriteTaskImpl, protected Behavior::RingBufferTask<RING_BUFFER> {
    public:
    TransferRingToMemory(
        Behavior::RingBufferTask<RING_BUFFER>::cable_type& indices,
        Behavior::RingBufferTask<RING_BUFFER>::const_cable_type& bounds,
        const std::size_t& vst_numInputs
        ) : WriteTaskImpl(vst_numInputs), SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds) {}
    protected:
    //multiple inheritance: overrides RingBufferTask
    virtual void write(const RING_BUFFER::value_type character) final {
        WriteTaskImpl::write(character);
    }
};
//multiple inheritance: destruction order
class RingBufferImpl : protected SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>, public TransferRingToMemory {
    public:
    //multiple inheritance: construction order
    RingBufferImpl(MemoryView::RingBufferBus<RING_BUFFER>& rB,MemoryView::ReaderBus<READ_BUFFER>& rd, const std::size_t& vst_numInputs) :
    rB(rB),
    TransferRingToMemory(std::get<0>(rB.cables),std::get<0>(rB.const_cables),vst_numInputs),
    SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>(rB.signal,_blocker,rd)
    {
        //multiple inheritance: refer to sub-routine in this.event_loop
        _thread = SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
    }
    ~RingBufferImpl() final{
        stop_requested=true;
        _thread.join();
    }
    void resetAndRestart() {
        clear_memorycontroller = true;
    }
    //Overriding PassthruSimpleController
    void event_loop(){
        while(!stop_requested){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                RingBufferTask::read_loop();
            }
            if (clear_memorycontroller) {
                auto previous = std::get<0>(rB.cables).getCurrentRef().load();
                std::get<0>(rB.cables).getThreadCurrentRef().store(--previous);
                clearMemoryController();
                clear_memorycontroller = false;
            }
            std::this_thread::yield();
        }
    }
    protected:
    bool stop_requested = false;//REMOVE: impl has to be in a valid state without stopping threads
    bool clear_memorycontroller = false;
    private:
    MemoryView::RingBufferBus<RING_BUFFER>& rB;
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};
}
