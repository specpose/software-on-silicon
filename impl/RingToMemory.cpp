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
using RING_BUFFER=std::array<BLINK_T,16>;
using MEMORY_CONTROLLER=std::vector<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>>;
using BLOCK=std::array<MEMORY_CONTROLLER::value_type,BLOCK_SIZE>;

class ReadTaskImpl : protected virtual SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& Memory,blocker_length_ct& Blocker)
    {}
    protected:
    virtual void outOfBounds(const typename reader_length_ct::arithmetic_type& current) final {
        *(current) = MEMORY_CONTROLLER::value_type{{0}};
    };
    virtual void busy(const typename reader_length_ct::arithmetic_type& current) final {
        *(current) = MEMORY_CONTROLLER::value_type{{9}};
    }
};

class ReaderImpl : public SOS::Behavior::Reader<BLOCK,MEMORY_CONTROLLER>,
                    private virtual ReadTaskImpl {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>& blockerbus):
    SOS::Behavior::Reader<BLOCK,MEMORY_CONTROLLER>(outside.signal, blockerbus.signal),
    ReadTaskImpl(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables),std::get<1>(blockerbus.cables)),
    SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER>(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables),std::get<1>(blockerbus.cables))
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

//multiple inheritance: destruction order
class RingBufferTaskImpl : private SOS::Behavior::RingBufferTask<RING_BUFFER>, protected SOS::Behavior::NonBlockingWriteTask<MEMORY_CONTROLLER> {
    public:
    RingBufferTaskImpl(
        SOS::Behavior::RingBufferTask<RING_BUFFER>::cable_type& indices,
        SOS::Behavior::RingBufferTask<RING_BUFFER>::const_cable_type& bounds
        ) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds), SOS::Behavior::NonBlockingWriteTask<MEMORY_CONTROLLER>(memorycontroller) {
            _blocker.signal.getWritingRef().clear();
            std::fill(std::begin(memorycontroller),std::end(memorycontroller),MEMORY_CONTROLLER::value_type{0});
            _blocker.signal.getWritingRef().test_and_set();
        }
    protected:
    virtual void transfer(const RING_BUFFER::value_type& character) {
        if (firstRun) {
            reserve(2*SAMPLE_RATE);
        } else if (character_count==character_threshold) {
            reserve(1*SAMPLE_RATE);
            character_count=0;
        }
        grow(std::tuple_size<RING_BUFFER::value_type>{});
        if (firstRun) {
            _blocker.signal.getResizingRef().clear();
            std::get<0>(_blocker.cables).getMCStartRef().store(std::begin(memorycontroller));
            std::get<0>(_blocker.cables).getMCEndRef().store(std::end(memorycontroller));
            std::get<1>(_blocker.cables).getBKStartRef().store(std::begin(memorycontroller));
            std::get<1>(_blocker.cables).getBKEndRef().store(std::begin(memorycontroller));
            _blocker.signal.getResizingRef().test_and_set();
            firstRun = false;
        }
        write(character);
        ++character_count;
    }
    private:
    void reserve(std::size_t add){
        while (!_blocker.signal.getReadingRef().test_and_set()){
            _blocker.signal.getReadingRef().clear();
            std::this_thread::yield();
        }
        _blocker.signal.getResizingRef().clear();
        const auto bk_start = std::distance(std::begin(memorycontroller),std::get<1>(_blocker.cables).getBKStartRef().load());
        const auto bk_end = std::distance(std::begin(memorycontroller),std::get<1>(_blocker.cables).getBKEndRef().load());
        _reserve+=add;
        memorycontroller.reserve(_reserve);
        std::get<0>(_blocker.cables).getMCStartRef().store(std::begin(memorycontroller));
        std::get<0>(_blocker.cables).getMCEndRef().store(std::end(memorycontroller));
        std::get<1>(_blocker.cables).getBKStartRef().store(std::begin(memorycontroller)+bk_start);
        std::get<1>(_blocker.cables).getBKEndRef().store(std::begin(memorycontroller)+bk_end);
        _blocker.signal.getResizingRef().test_and_set();
    }
    void grow(MEMORY_CONTROLLER::difference_type add){
        for (std::size_t i = 0; i < add; i++)
            memorycontroller.push_back(MEMORY_CONTROLLER::value_type{0});
        std::get<0>(_blocker.cables).getMCEndRef().store(std::end(memorycontroller));
    };
    virtual void write(const typename RING_BUFFER::value_type& character) {
        const auto bk_start = std::get<1>(_blocker.cables).getBKStartRef().load();
        if ( std::tuple_size<RING_BUFFER::value_type>{} <= std::distance(bk_start, std::get<0>(_blocker.cables).getMCEndRef().load()) ) {
            _blocker.signal.getWritingRef().clear();
            std::get<1>(_blocker.cables).getBKEndRef().store(bk_start+std::tuple_size<RING_BUFFER::value_type>{});
            _blocker.signal.getWritingRef().test_and_set();
            auto writerPos = std::get<1>(_blocker.cables).getBKStartRef().load();
            writerPos = std::copy(std::begin(character),std::end(character),writerPos);
            _blocker.signal.getWritingRef().clear();
            std::get<1>(_blocker.cables).getBKStartRef().store(writerPos);
            _blocker.signal.getWritingRef().test_and_set();
        } else {
            SFA::util::logic_error(SFA::util::error_code::CharacterWriteRangeFailed,__FILE__,__func__);
        }
        if (std::distance(std::get<1>(_blocker.cables).getBKStartRef().load(),std::get<1>(_blocker.cables).getBKEndRef().load())!=0)
            SFA::util::logic_error(SFA::util::error_code::UnexpectedWritesLeft,__FILE__,__func__);
    }
    bool firstRun = true;
    std::size_t character_count = 0;
    const std::size_t character_threshold = (SAMPLE_RATE-std::tuple_size<RING_BUFFER::value_type>{})/std::tuple_size<RING_BUFFER::value_type>{};

    std::size_t _reserve = 0;
    MEMORY_CONTROLLER memorycontroller{};
};
//multiple inheritance: destruction order
class RingBufferImpl : public SOS::Behavior::RingBuffer<RING_BUFFER>, private RingBufferTaskImpl, public SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>> {
    public:
    //multiple inheritance: construction order
    RingBufferImpl(SOS::MemoryView::RingBufferBus<RING_BUFFER>& rB,SOS::MemoryView::ReaderBus<BLOCK>& rd) :
    SOS::Behavior::RingBuffer<RING_BUFFER>(rB.signal),
    RingBufferTaskImpl(std::get<0>(rB.cables),std::get<0>(rB.const_cables)),
    RingBufferTask(std::get<0>(rB.cables),std::get<0>(rB.const_cables)),
    SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>(rB.signal,rd,_blocker)
    {
        //multiple inheritance: PassthruSimpleController, not ReaderImpl
        _thread = start(this);
    }
    ~RingBufferImpl() {
        destroy(_thread);
    }
    //using SOS::Behavior::RingBuffer<RING_BUFFER>::event_loop;
    virtual void event_loop() final {
        SOS::Behavior::RingBuffer<RING_BUFFER>::event_loop();
    }
    //using RingBufferTaskImpl::transfer;
    virtual void transfer(const RING_BUFFER::value_type& character) final {
        RingBufferTaskImpl::transfer(character);
    }
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread;
};