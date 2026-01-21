#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <iostream>
#include "software-on-silicon/simulation_helpers.hpp"
#include <chrono>

#include "Sample.cpp"
#define STORAGE_SIZE 10000
#define SAMPLE_TYPE char
#define NUM_CHANNELS 1
using MEMORY_CONTROLLER=std::array<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>,STORAGE_SIZE>;//INTERLEAVED
using BLOCK=std::array<MEMORY_CONTROLLER::value_type,1000>;

class ReadTaskImpl : private virtual SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER> {
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
                *current = *readerPos;
                readerPos++;
                ++current;
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
    ReadTaskImpl(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables)),
    SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.const_cables))
    {
        //multiple inheritance: not ambiguous
        //_thread = SOS::Behavior::Reader<MEMORY_CONTROLLER>::start(this);
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
class WriteTaskImpl : protected SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<MEMORY_CONTROLLER>() {
        this->memorycontroller.fill(MEMORY_CONTROLLER::value_type{'-'});
    }
    ~WriteTaskImpl(){}
    protected:
    virtual void write(MEMORY_CONTROLLER::value_type& character) final {
        _blocker.signal.getWritingRef().clear();
        SOS::Behavior::WriteTask<MEMORY_CONTROLLER>::write(character);
        _blocker.signal.getWritingRef().test_and_set();
    }
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
        int counter = 0;
        bool blink = true;
        while(is_running()){
        const auto start = high_resolution_clock::now();
        MEMORY_CONTROLLER::value_type data;
        if (blink)
            data = MEMORY_CONTROLLER::value_type{'*'};
        else
            data = MEMORY_CONTROLLER::value_type{'_'};
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
