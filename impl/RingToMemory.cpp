#include "software-on-silicon/error.hpp"
#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "software-on-silicon/MemoryController.hpp"

using namespace SOS;

namespace SOSFloat {
using SAMPLE_SIZE = float;
using RING_BUFFER = std::array<std::tuple<SOS::MemoryView::Contiguous<SAMPLE_SIZE>**,unsigned int,unsigned int>,2>;//0:[maxSamplesPerProcess][vst_numInputs], 1: vst_processSamples, 2: ara_samplePosition
using MEMORY_CONTROLLER=std::vector<SOS::MemoryView::Contiguous<SAMPLE_SIZE>*>;//10000
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
        const auto readOffset = _offset.getReadOffsetRef().load();
        if (readOffset<0)
            throw SFA::util::runtime_error("Negative read offset supplied",__FILE__,__func__);
        const auto readerStart = _memorycontroller_size.getBKStartRef().load();
        const auto readerEnd = _memorycontroller_size.getBKEndRef().load();
        auto readerPos = readerStart+readOffset;
        while (current!=end){
            if (!wait()) {
                //if the distance of the lval from its start is bigger than
                //the (the rval offset to rval end)
                if (std::distance(start,current)>=std::distance(readerStart+readOffset,readerEnd)){
                    *current = 0.0;
                } else {
                    *current = (**readerPos)[channel];
                    readerPos++;
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
    std::thread _thread;
};
class WriteTaskImpl : public SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl(const std::size_t vst_numInputs) :
    SOS::Behavior::WriteTask<MEMORY_CONTROLLER>(vst_numInputs),
    _vst_numInputs(vst_numInputs)
    {
        resize(0);
    }
    virtual void resize(MEMORY_CONTROLLER::difference_type newsize){
        memorycontroller.reserve(newsize);
        while(memorycontroller.size()<newsize){
            auto entry = new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(_vst_numInputs);
            memorycontroller.push_back(entry);
        }
        std::get<0>(_blocker.cables).getBKStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef().store(memorycontroller.end());
    };
    //helper function, not inherited
    void write(const RING_BUFFER::value_type character) {
        resize(std::get<2>(character)+std::get<1>(character));//offset + length
        if (std::distance(std::get<0>(_blocker.cables).getBKStartRef().load(),std::get<0>(_blocker.cables).getBKEndRef().load())<
        std::get<2>(character)+std::get<1>(character))
            throw SFA::util::runtime_error("Writer tried to write beyond memorycontroller bounds",__FILE__,__func__);
        writerPos = std::get<0>(_blocker.cables).getBKStartRef().load() + std::get<2>(character);
        for(std::size_t i=0;i<std::get<1>(character);i++)
            SOS::Behavior::WriteTask<MEMORY_CONTROLLER>::write((*std::get<0>(character)));
    }
    //not inherited
    void clearMemoryController() {
        for(auto entry : memorycontroller)
            if (entry)
                delete entry;
        memorycontroller.clear();
        memorycontroller.resize(0);
    }
    private:
    std::size_t _vst_numInputs;
};
class TransferRingToMemory : protected Behavior::RingBufferTask<RING_BUFFER>, protected WriteTaskImpl {
    public:
    TransferRingToMemory(
        Behavior::RingBufferTask<RING_BUFFER>::cable_type& indices,
        Behavior::RingBufferTask<RING_BUFFER>::const_cable_type& bounds,
        std::size_t vst_numInputs
        ) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds), WriteTaskImpl(vst_numInputs) {}
    protected:
    //multiple inheritance: ambiguous override!
    virtual void write(const RING_BUFFER::value_type character) final {
        throw SFA::util::logic_error("write is called",__FILE__,__func__);
        _blocker.signal.getNotifyRef().clear();
        WriteTaskImpl::write(character);
        _blocker.signal.getNotifyRef().test_and_set();
    }
};
class RingBufferImpl : public TransferRingToMemory, protected SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>> {
    public:
    RingBufferImpl(MemoryView::RingBufferBus<RING_BUFFER>& rB,MemoryView::ReaderBus<READ_BUFFER>& rd, std::size_t vst_numInputs) :
    TransferRingToMemory(std::get<0>(rB.cables),std::get<0>(rB.const_cables),vst_numInputs),
    SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>(rB.signal,_blocker,rd)
    {
        //multiple inheritance: ambiguous base-class call
        _thread = SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
    }
    ~RingBufferImpl() final{
        stop_requested=true;
        _thread.join();
    }
    //Overriding PassthruSimpleController
    void event_loop(){
        while(!stop_requested){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                RingBufferTask::read_loop();
            }
        }
    }
    protected:
    bool stop_requested = false;
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};
}
