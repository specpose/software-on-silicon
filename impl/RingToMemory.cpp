#define OLD_ITERATOR_DEBUG_LEVEL _ITERATOR_DEBUG_LEVEL//DISABLE
#define _ITERATOR_DEBUG_LEVEL 0//DISABLE

#include "software-on-silicon/error.hpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "software-on-silicon/memorycontroller_helpers.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <iostream>
#include "software-on-silicon/simulation_helpers.hpp"

namespace SOSFloat {
using SAMPLE_SIZE = float;
using RING_BUFFER = std::array<std::tuple<SOS::MemoryView::Contiguous<SAMPLE_SIZE>**,unsigned int,unsigned int>,2>;//400//0:[maxSamplesPerProcess][vst_numInputs], 1: vst_processSamples, 2: ara_samplePosition
using MEMORY_CONTROLLER=std::vector<SOS::MemoryView::Contiguous<SAMPLE_SIZE>*>;
//using READ_BUFFER=std::vector<SOS::MemoryView::ARAChannel<SOSFloat::SAMPLE_SIZE>>;
}
//main branch: Copy Start from MemoryController.cpp
namespace SOS {
    namespace MemoryView {
        template<> struct reader_traits<SOSFloat::MEMORY_CONTROLLER> : public SFA::DeductionGuide<std::vector<SOS::MemoryView::ARAChannel<typename std::remove_pointer<typename SOSFloat::MEMORY_CONTROLLER::value_type>::type::value_type>>> {};
        //template<> struct reader_traits<SOSFloat::MEMORY_CONTROLLER> : public SFA::DeductionGuide<std::vector<SOS::MemoryView::ARAChannel<SOSFloat::SAMPLE_SIZE>>> {};
        //template<> struct reader_traits<SOSFloat::MEMORY_CONTROLLER> {
        //using input_container_type = std::vector<SOS::MemoryView::ARAChannel<typename std::remove_pointer<typename SOSFloat::MEMORY_CONTROLLER::value_type>::type::value_type>>;
        //};
    }
}

using namespace SOS;
namespace SOSFloat {
class ReadTaskImpl : private virtual SOS::Behavior::ReadTask<MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) 
    //: SOS::Behavior::ReadTask<MEMORY_CONTROLLER,READ_SIZE>(Length, Offset, blockercable)
    {}
    protected:
    void read(){
        for (std::size_t channel=0;channel<_size.size();channel++){
        //readbuffer
        const auto start = _size[channel].getReadBufferStartRef();
        auto current = start;
        const auto end = _size[channel].getReadBufferAfterLastRef();
        //memorycontroller
        auto readOffset = _offset.getReadOffsetRef();
        if (readOffset<0)
            throw SFA::util::runtime_error("Negative read offset supplied",__FILE__,__func__);
        while (current!=end){
            if (!wait()) {
                auto readerStart = _memorycontroller_size.getBKStartRef();
                readerStart += readOffset;
                const auto readerEnd = _memorycontroller_size.getBKEndRef();
                //if the distance of the lval from its start is bigger than
                //the (the rval offset to rval end)
                if (std::distance(start,current)>=std::distance(readerStart,readerEnd)){
                    *current = 0.0;
                } else {
                    *current = (**(readerStart))[channel];
                    readOffset++;
                }
                ++current;
                wait_acknowledge();
            }
            std::this_thread::yield();
        }
        }
    }
};
//main branch: Copy End from MemoryController.cpp

class ReaderImpl : public SOS::Behavior::Reader<MEMORY_CONTROLLER>,
                    private virtual ReadTaskImpl {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>& blockerbus):
    SOS::Behavior::Reader<MEMORY_CONTROLLER>(outside, blockerbus),
    ReadTaskImpl(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables)),
    SOS::Behavior::ReadTask<MEMORY_CONTROLLER>(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables))
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
        std::get<0>(_blocker.cables).getBKStartRef() = memorycontroller.begin();
        std::get<0>(_blocker.cables).getBKEndRef() = memorycontroller.end();
        ara_sampleCount = memorycontroller.size();
        for(auto& sample : memorycontroller)
            if (sample->size()!=_vst_numInputs)
                throw SFA::util::logic_error("Memorycontroller resize error",__FILE__,__func__);
    };
    //not inherited: overload
    void write(const RING_BUFFER::value_type character) {
        _blocker.signal.getUpdatedRef().clear();
        resize(std::get<2>(character)+std::get<1>(character));//offset + length
        _blocker.signal.getUpdatedRef().test_and_set();
        if (std::distance(std::get<0>(_blocker.cables).getBKStartRef(),std::get<0>(_blocker.cables).getBKEndRef())<
        std::get<2>(character)+std::get<1>(character))
            throw SFA::util::runtime_error("Writer tried to write beyond memorycontroller bounds",__FILE__,__func__);
        writerPos = std::get<0>(_blocker.cables).getBKStartRef() + std::get<2>(character);
        for(std::size_t i=0;i<std::get<1>(character);i++){
            _blocker.signal.getUpdatedRef().clear();
            SOS::Behavior::WriteTask<MEMORY_CONTROLLER>::write((std::get<0>(character))[i]);
            _blocker.signal.getUpdatedRef().test_and_set();
        }
    }
    //not inherited
    void clearMemoryController() {
        _blocker.signal.getUpdatedRef().clear();
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
        std::get<0>(_blocker.cables).getBKStartRef() = (memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef() = (memorycontroller.end());
        _blocker.signal.getUpdatedRef().test_and_set();
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
    private:
    //multiple inheritance: overrides RingBufferTask
    virtual void write(const RING_BUFFER::value_type character) final {
        WriteTaskImpl::write(character);
    }
};
//multiple inheritance: destruction order
class RingBufferImpl : public SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>, public TransferRingToMemory {
    public:
    //multiple inheritance: construction order
    RingBufferImpl(MemoryView::RingBufferBus<RING_BUFFER>& rB,MemoryView::ReaderBus<SOS::MemoryView::reader_traits<MEMORY_CONTROLLER>::input_container_type>& rd, const std::size_t& vst_numInputs) :
    rB(rB),
    TransferRingToMemory(std::get<0>(rB.cables),std::get<0>(rB.const_cables),vst_numInputs),
    SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>(rB.signal,rd,_blocker)
    {
        //multiple inheritance: PassthruSimpleController, not ReaderImpl
        //_thread = SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
        _thread = start(this);
    }
    ~RingBufferImpl() final{
        destroy(_thread);
    }
    void resetAndRestart() {
        clear_memorycontroller = true;
    }
    //multiple inheritance: Overriding RingBufferImpl, not ReaderImpl
    void event_loop(){
        while(is_running()){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                RingBufferTask::read_loop();
            }
            if (clear_memorycontroller) {
                //stop and omit the pending transfer writes
                auto previous = std::get<0>(rB.cables).getCurrentRef();
                std::get<0>(rB.cables).getThreadCurrentRef() = --previous;
                while(!_blocker.signal.getAcknowledgeRef().test_and_set()){
                    _blocker.signal.getAcknowledgeRef().clear();
                    std::this_thread::yield();
                }//acknowledge => finished a pending read
                clearMemoryController();
                clear_memorycontroller = false;
            }
            std::this_thread::yield();
        }
        finished();
    }
    protected:
    bool clear_memorycontroller = false;
    private:
    MemoryView::RingBufferBus<RING_BUFFER>& rB;
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};
}

#define _ITERATOR_DEBUG_LEVEL OLD_ITERATOR_DEBUG_LEVEL//DISABLE
#undef OLD_ITERATOR_DEBUG_LEVEL//DISABLE