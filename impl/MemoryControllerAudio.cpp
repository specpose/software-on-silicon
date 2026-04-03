#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <iostream>
#include <chrono>

using namespace std::chrono;

//#include "Sample.cpp"
//using MEMORY_CONTROLLER=std::array<SOS::MemoryView::sample<SAMPLE_TYPE,NUM_CHANNELS>,STORAGE_SIZE>;
using BLOCK=std::array<MEMORY_CONTROLLER::value_type,BLOCK_SIZE>;

class ReadTaskImpl : protected virtual SOS::Behavior::ReadTask<BLOCK,MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& Memory,blocker_length_ct& Blocker)
    {}
    protected:
    virtual void outOfBounds(const typename reader_length_ct::arithmetic_type& current) final {
        SFA::util::runtime_error(SFA::util::error_code::ReadindexOutOfBounds,__FILE__,__func__);
    };
    virtual void busy(const typename reader_length_ct::arithmetic_type& current) final {
//        *(current) = MEMORY_CONTROLLER::value_type{{0,0,0,0,9}};
        *(current) = MEMORY_CONTROLLER::value_type{{0,0}};
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

//MemoryControllerAudioOverride start
//multiple inheritance: destruction order
class WritePriorityImpl2 : public SOS::Behavior::PassthruAsyncController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>, private SOS::Behavior::NonBlockingWriteTask<MEMORY_CONTROLLER> {
public:
    //multiple inheritance: construction order
    WritePriorityImpl2(
        SOS::MemoryView::ReaderBus<BLOCK>& passThruHostMem
    ) :
    SOS::Behavior::NonBlockingWriteTask<MEMORY_CONTROLLER>(memorycontroller),
    PassthruAsyncController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER> >(passThruHostMem,_blocker)
    {
        _blocker.signal.getWritingRef().clear();
        std::fill(std::begin(memorycontroller),std::end(memorycontroller),MEMORY_CONTROLLER::value_type{{0,0}});
        _blocker.signal.getWritingRef().test_and_set();
        //multiple inheritance: starts PassthruAsync, not ReaderImpl
        //_thread = PassthruAsync<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
        _thread = start(this);
    };
    virtual ~WritePriorityImpl2(){
        destroy(_thread);
    };
    //multiple inheritance: Overriding PassThru not ReaderImpl
    void event_loop(){
        snd_pcm_uframes_t period_size = MAX_READ;

        auto driver = SOS::Audio::Linux::init(rate, &period_size, SND_PCM_ACCESS_MMAP_INTERLEAVED, true);
        SOS::Audio::Linux::start_pcm(std::get<0>(driver));
        std::size_t frames_read = 0;
        while (frames_read + MAX_BLINK <= STORAGE_SIZE) {
            write(std::get<0>(driver), frames_read);
        }
        SOS::Audio::Linux::destroy(driver);
    }
private:
    virtual void write(snd_pcm_t *handle, std::size_t& frames_read) {
        auto writerPos = std::get<1>(_blocker.cables).getBKStartRef().load();
        if (writerPos!=std::get<0>(_blocker.cables).getMCEndRef().load()) {
            _blocker.signal.getWritingRef().clear();
            std::get<1>(_blocker.cables).getBKEndRef().store(std::get<1>(_blocker.cables).getBKEndRef().load()+MAX_BLINK);
            //*writerPos=character;
            SOS::Audio::Linux::record_blink_mmapinterleaved(*(reinterpret_cast<std::array<SOS::MemoryView::sample<SAMPLE_TYPE, NUM_CHANNELS>,STORAGE_SIZE>*>(writerPos)), handle, frames_read);
            writerPos += MAX_BLINK;
            std::get<1>(_blocker.cables).getBKStartRef().store(writerPos);
            _blocker.signal.getWritingRef().test_and_set();
        } else {
            SFA::util::logic_error(SFA::util::error_code::WriterBufferFull,__FILE__,__func__);
        }
    }
    int counter = 0;
    bool blink = true;
    MEMORY_CONTROLLER memorycontroller{};

    std::thread _thread;
};
//MemoryControllerAudioOverride end