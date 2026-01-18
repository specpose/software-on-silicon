#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/memorycontroller_helpers.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <iostream>
#include "software-on-silicon/simulation_helpers.hpp"
#include <chrono>

#include "Sample.cpp"
#define STORAGE_SIZE 10000
using CHANNELS_DATA=std::array<SOS::MemoryView::sample<char,1>,STORAGE_SIZE>;//INTERLEAVED
#define READ_SIZE 1000
namespace SOS {
    namespace MemoryView {
        template<> struct reader_traits<CHANNELS_DATA> : public SFA::DeductionGuide<std::array<CHANNELS_DATA::value_type,READ_SIZE>> {};
    }
}
using namespace SOS;
class ReadTaskImpl : private virtual SOS::Behavior::ReadTask<CHANNELS_DATA> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) 
    {}
    protected:
    virtual void read() {
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
                *current = *(readerPos++);
                ++current;
                wait_acknowledge();
            }
            std::this_thread::yield();
        }
    }
};

class ReaderImpl : public SOS::Behavior::Reader<CHANNELS_DATA>,
                    private virtual ReadTaskImpl {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus<CHANNELS_DATA>& blockerbus):
    SOS::Behavior::Reader<CHANNELS_DATA>(outside, blockerbus),
    ReadTaskImpl(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables)),
    SOS::Behavior::ReadTask<CHANNELS_DATA>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables))
    {
        //multiple inheritance: not ambiguous
        //_thread = SOS::Behavior::Reader<CHANNELS_DATA>::start(this);
        _thread = start(this);
    }
    ~ReaderImpl(){
        destroy(_thread);
    }
    virtual void restart() final { _thread = SOS::Behavior::Stoppable::start(this); }
    public:
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
    private:
    virtual void read() final {
            ReadTaskImpl::read();
    };
    std::thread _thread;
};
class WriteTaskImpl : protected SOS::Behavior::WriteTask<CHANNELS_DATA> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<CHANNELS_DATA>() {
        this->memorycontroller.fill(CHANNELS_DATA::value_type{'-'});
    }
    ~WriteTaskImpl(){}
    protected:
    virtual void write(CHANNELS_DATA::value_type& character) final {
        _blocker.signal.getWritingRef().clear();
        SOS::Behavior::WriteTask<CHANNELS_DATA>::write(character);
        _blocker.signal.getWritingRef().test_and_set();
    }
};
using namespace std::chrono;

//multiple inheritance: destruction order
class WritePriorityImpl : public SOS::Behavior::PassthruAsyncController<ReaderImpl, SOS::MemoryView::BlockerBus<CHANNELS_DATA>>, private WriteTaskImpl {
    public:
    //multiple inheritance: construction order
    WritePriorityImpl(
        SOS::MemoryView::ReaderBus<SOS::MemoryView::reader_traits<CHANNELS_DATA>::input_container_type>& passThruHostMem
        ) :
        WriteTaskImpl(),
        PassthruAsyncController<ReaderImpl, SOS::MemoryView::BlockerBus<CHANNELS_DATA> >(passThruHostMem,_blocker)
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
        int counter = 0;
        bool blink = true;
        while(is_running()){
        const auto start = high_resolution_clock::now();
        CHANNELS_DATA::value_type data;
        if (blink)
            data = CHANNELS_DATA::value_type{'*'};
        else
            data = CHANNELS_DATA::value_type{'_'};
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
