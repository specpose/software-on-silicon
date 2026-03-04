#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <vector>
#include <chrono>

using namespace std::chrono;

#include "Sample.cpp"
using BLINK_T = std::array<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>,MAX_BLINK>;
using RING_BUFFER=std::array<BLINK_T,2>;
using MEMORY_CONTROLLER=std::vector<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>>;
using BLOCK=std::array<MEMORY_CONTROLLER::value_type,BLOCK_SIZE>;

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
                const auto bk_start = _memorycontroller_size.getMCStartRef().load();
                const auto bk_end = _memorycontroller_size.getMCEndRef().load();
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
class WriteTaskImpl : protected SOS::Behavior::NonBlockingWriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl() : SOS::Behavior::NonBlockingWriteTask<MEMORY_CONTROLLER>() {
        _blocker.signal.getWritingRef().clear();
        std::fill(std::begin(memorycontroller),std::end(memorycontroller),MEMORY_CONTROLLER::value_type{0});
        _blocker.signal.getWritingRef().test_and_set();
    }
    ~WriteTaskImpl(){}
};
//multiple inheritance: destruction order
class RingBufferTaskImpl : protected SOS::Behavior::RingBufferTask<RING_BUFFER>, public WriteTaskImpl {
    public:
    RingBufferTaskImpl(
        SOS::Behavior::RingBufferTask<RING_BUFFER>::cable_type& indices,
        SOS::Behavior::RingBufferTask<RING_BUFFER>::const_cable_type& bounds
        ) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds), WriteTaskImpl{} {
            resize(old_reserve);
        }
    private:
    virtual void resize(std::size_t newsize){
        _blocker.signal.getResizingRef().clear();
        const auto bk_start = std::distance(std::begin(memorycontroller),std::get<1>(_blocker.cables).getBKStartRef().load());
        const auto bk_end = std::distance(std::begin(memorycontroller),std::get<1>(_blocker.cables).getBKEndRef().load());
        memorycontroller.reserve(newsize);
        std::get<0>(_blocker.cables).getMCStartRef().store(std::begin(memorycontroller));
        std::get<0>(_blocker.cables).getMCEndRef().store(std::end(memorycontroller));
        std::get<1>(_blocker.cables).getBKStartRef().store(std::begin(memorycontroller)+bk_start);
        std::get<1>(_blocker.cables).getBKEndRef().store(std::begin(memorycontroller)+bk_end);
        _blocker.signal.getResizingRef().test_and_set();
    }
    virtual void grow(MEMORY_CONTROLLER::difference_type add){
        //_blocker.signal.getResizingRef().clear();//not needed
        //const auto bk_start = std::distance(std::begin(memorycontroller),std::get<1>(_blocker.cables).getBKStartRef().load());
        //const auto bk_end = std::distance(std::begin(memorycontroller),std::get<1>(_blocker.cables).getBKEndRef().load());
        for (std::size_t i = 0; i < add; i++)
            memorycontroller.push_back(MEMORY_CONTROLLER::value_type{0});
        //std::get<0>(_blocker.cables).getMCStartRef().store(std::begin(memorycontroller));
        //std::get<0>(_blocker.cables).getMCEndRef().store(std::end(memorycontroller));
        //std::get<1>(_blocker.cables).getBKStartRef().store(std::begin(memorycontroller)+bk_start);
        //std::get<1>(_blocker.cables).getBKEndRef().store(std::begin(memorycontroller)+bk_end);
        //_blocker.signal.getResizingRef().test_and_set();//not needed
    };
    //overrides RingBufferTask::transfer and remaps to WriteTaskImpl::write
    virtual void transfer(const RING_BUFFER::value_type& character) final {
        const auto now = high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last).count() > 0) {
            old_reserve+=SAMPLE_RATE;
            resize(old_reserve);
            last = now;
        }
        grow(std::tuple_size<RING_BUFFER::value_type>{});
        std::size_t count = 0;
        block(std::tuple_size<RING_BUFFER::value_type>{});
        auto c = std::begin(character);
        while(std::get<1>(_blocker.cables).getBKStartRef().load()!=std::get<1>(_blocker.cables).getBKEndRef().load()){
            /*if ( std::distance(writerPos,std::end(memorycontroller)) < std::tuple_size<RING_BUFFER::value_type>{} )
                SFA::util::runtime_error(SFA::util::error_code::WriterTriedToWriteBeyondMemorycontrollerBounds,__FILE__,__func__);
            std::cout<<"Offset: "<<std::distance(std::begin(memorycontroller),writerPos)<<std::endl;
            std::cout<<"BKLength size: "<< std::distance(std::get<0>(_blocker.cables).getMCStartRef().load(),std::get<0>(_blocker.cables).getMCEndRef().load())<<std::endl;*/
            WriteTaskImpl::write(*c++);
            count++;
        }
        if (count!=std::tuple_size<RING_BUFFER::value_type>{})
            SFA::util::logic_error(SFA::util::error_code::WroteTooMuchOrTooLittle,__FILE__,__func__);
        if (std::distance(std::get<1>(_blocker.cables).getBKStartRef().load(),std::get<1>(_blocker.cables).getBKEndRef().load())!=0)
            SFA::util::logic_error(SFA::util::error_code::UnexpectedWritesLeft,__FILE__,__func__);
    }
    std::chrono::time_point<std::chrono::high_resolution_clock> last = high_resolution_clock::now();
    std::size_t old_reserve = 2 * SAMPLE_RATE;
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
    std::thread _thread;
};