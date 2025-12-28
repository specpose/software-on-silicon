#include <iostream>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/memorycontroller_helpers.hpp"
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <iostream>
#include "software-on-silicon/simulation_helpers.hpp"
#include <chrono>

namespace SOSFloat {
using SAMPLE_SIZE = float;
#define STORAGE_SIZE 10000
using MEMORY_CONTROLLER=std::array<SOS::MemoryView::Contiguous<SAMPLE_SIZE>*,STORAGE_SIZE>;
//using READ_BUFFER=std::vector<SOS::MemoryView::ARAChannel<SOSFloat::SAMPLE_SIZE>>;
}
namespace SOS {
    namespace MemoryView {
        template<> struct reader_traits<SOSFloat::MEMORY_CONTROLLER> : public SFA::DeductionGuide<std::vector<SOS::MemoryView::ARAChannel<typename std::remove_pointer<typename SOSFloat::MEMORY_CONTROLLER::value_type>::type::value_type>>> {};
        //template<> struct reader_traits<SOSFloat::MEMORY_CONTROLLER> : public SFA::DeductionGuide<std::vector<SOS::MemoryView::ARAChannel<SOSFloat::SAMPLE_SIZE>>> {};
        //template<> struct reader_traits<SOSFloat::MEMORY_CONTROLLER> {
        //using input_container_type = std::vector<SOS::MemoryView::ARAChannel<typename std::remove_pointer<typename SOSFloat::MEMORY_CONTROLLER::value_type>::type::value_type>>;
        //};
    }
}

using namespace SOS::MemoryView;
namespace SOSFloat {
class ReadTaskImpl : private virtual SOS::Behavior::ReadTask<MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) 
    {}
    protected:
    virtual void read(){
        for (std::size_t channel=0;channel<_size.size();channel++){
        const auto start = _size[channel].getReadBufferStartRef().load();
        auto current = start;
        const auto end = _size[channel].getReadBufferAfterLastRef().load();
        auto readOffset = _offset.getReadOffsetRef().load();
        if (readOffset<0)
            SFA::util::runtime_error(SFA::util::error_code::NegativeReadoffsetSupplied,__FILE__,__func__);
        while (current!=end){
            if (!wait()) {
                if (std::distance(_memorycontroller_size.getBKStartRef().load(), _memorycontroller_size.getBKEndRef().load())
                    < (std::distance(start, end) + readOffset))
                    SFA::util::runtime_error(SFA::util::error_code::ReadindexOutOfBounds, __FILE__, __func__);
                //readOffset stays valid with resize (grow), but not clearMemoryController
                auto readerPos = _memorycontroller_size.getBKStartRef().load()+readOffset;
                *current = (**readerPos)[channel];
                readOffset++;
                ++current;
                wait_acknowledge();
            }
            std::this_thread::yield();
        }
        }
    }
};
class ReaderImpl : public SOS::Behavior::Reader<MEMORY_CONTROLLER>,
                    private virtual ReadTaskImpl {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>& blockerbus):
    SOS::Behavior::Reader<MEMORY_CONTROLLER>(outside, blockerbus),
    ReadTaskImpl(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables)),
    SOS::Behavior::ReadTask<MEMORY_CONTROLLER>(std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables))
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
    WriteTaskImpl(const std::size_t& vst_numInputs) : SOS::Behavior::WriteTask<MEMORY_CONTROLLER>{} {
        _blocker.signal.getUpdatedRef().clear();
        for(auto& entry : memorycontroller)
            entry = new SOS::MemoryView::Contiguous<typename std::remove_pointer<MEMORY_CONTROLLER::value_type>::type::value_type>(5);
        std::get<0>(_blocker.cables).getBKStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef().store(memorycontroller.end());
        _blocker.signal.getUpdatedRef().test_and_set();
    }
    ~WriteTaskImpl(){
        _blocker.signal.getUpdatedRef().clear();
        for(auto& entry : memorycontroller)
            if (entry)
                delete entry;
        _blocker.signal.getUpdatedRef().test_and_set();
    }
    protected:
    virtual void write(const MEMORY_CONTROLLER::value_type character) override {
        _blocker.signal.getUpdatedRef().clear();
        SOS::Behavior::WriteTask<MEMORY_CONTROLLER>::write(character);
        _blocker.signal.getUpdatedRef().test_and_set();
    }
};
using namespace std::chrono;

//multiple inheritance: destruction order
class WritePriorityImpl : public SOS::Behavior::PassthruAsyncController<ReaderImpl, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>>, private WriteTaskImpl {
    public:
    //multiple inheritance: construction order
    WritePriorityImpl(
        SOS::MemoryView::ReaderBus<SOS::MemoryView::reader_traits<MEMORY_CONTROLLER>::input_container_type>& passThruHostMem,
        const std::size_t& vst_numInputs
        ) :
        WriteTaskImpl(vst_numInputs),
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
        std::remove_pointer<MEMORY_CONTROLLER::value_type>::type data = SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5);
        if (blink){
            //auto entry = new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5);
            (data)[0]=0.0;//HACK: hard-coded channel 5
            (data)[1]=0.0;//HACK: hard-coded channel 5
            (data)[2]=0.0;//HACK: hard-coded channel 5
            (data)[3]=0.0;//HACK: hard-coded channel 5
            (data)[4]=1.0;//HACK: hard-coded channel 5
            //data=entry;
        }else{
            //auto entry = new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5);
            (data)[0]=0.0;//HACK: hard-coded channel 5
            (data)[1]=0.0;//HACK: hard-coded channel 5
            (data)[2]=0.0;//HACK: hard-coded channel 5
            (data)[3]=0.0;//HACK: hard-coded channel 5
            (data)[4]=0.0;//HACK: hard-coded channel 5
            //data=entry;
        }
        write(&data);
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
}
