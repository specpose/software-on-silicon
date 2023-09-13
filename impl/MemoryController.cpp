#include "software-on-silicon/error.hpp"
#include <iostream>
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <chrono>

namespace SOSFloat {
using SAMPLE_SIZE = float;
using MEMORY_CONTROLLER=std::array<SOS::MemoryView::Contiguous<SAMPLE_SIZE>*,10000>;
using READ_BUFFER=std::vector<SOS::MemoryView::ARAChannel<SOSFloat::SAMPLE_SIZE>>;

using namespace SOS::MemoryView;
class ReadTaskImpl : public SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(SOS::MemoryView::HandShake& stop_token,reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) :
    stop_token(stop_token),
    SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER>(Length, Offset, blockercable)
    {}
    protected:
    void read(){
        for (std::size_t channel=0;channel<_size.size();channel++){
        const auto start = _size[channel].getReadBufferStartRef().load();
        auto current = start;
        const auto end = _size[channel].getReadBufferAfterLastRef().load();
        auto readOffset = _offset.getReadOffsetRef().load();
        if (readOffset<0)
            throw SFA::util::runtime_error("Negative read offset supplied",__FILE__,__func__);
        while (current!=end && !exit_loop()){
            if (!wait()) {
                if (std::distance(_memorycontroller_size.getBKStartRef().load(), _memorycontroller_size.getBKEndRef().load())
                    < (std::distance(start, end) + readOffset))
                    throw SFA::util::runtime_error("Read index out of bounds", __FILE__, __func__);
                //readOffset stays valid with resize (grow), but not clearMemoryController
                auto readerPos = _memorycontroller_size.getBKStartRef().load()+readOffset;
                *current = (**readerPos)[channel];
                readOffset++;
                ++current;
                acknowledge();
            }
        }
        }
    }
    virtual void acknowledge()=0;
    virtual bool exit_loop()=0;
    private:
    SOS::MemoryView::HandShake& stop_token;
};
class ReaderImpl : public SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>,
                    private ReadTaskImpl {
    public:
    ReaderImpl(bus_type& blockerbus,SOS::MemoryView::ReaderBus<READ_BUFFER>& outside):
    SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>(blockerbus, outside),
    ReadTaskImpl(Loop::stop_token,std::get<1>(outside.cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables))
    {
        //multiple inheritance: not ambiguous
        //_thread = SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>::start(this);
        _thread = start(this);
    }
    ~ReaderImpl(){
        _thread.join();
    }
    virtual void read() final {ReadTaskImpl::read();};
    virtual void acknowledge() final {SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>::acknowledge();};
    virtual bool wait() final {return SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>::wait();};
    virtual bool exit_loop() final {return SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>::exit_loop();};
    private:
    std::thread _thread;
};
class WriteTaskImpl : public SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
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
class WritePriorityImpl : public PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>, public WriteTaskImpl, public SOS::Behavior::Loop {
    public:
    //multiple inheritance: construction order
    WritePriorityImpl(
        SOS::MemoryView::ReaderBus<READ_BUFFER>& passThruHostMem,
        const std::size_t& vst_numInputs
        ) :
        WriteTaskImpl(vst_numInputs),
        PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>(_blocker,passThruHostMem)
        {
            //multiple inheritance: starts PassthruThread, not ReaderImpl
            //_thread = PassthruThread<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
            _thread = start(this);
        };
    virtual ~WritePriorityImpl(){
        _child.stop();//ALWAYS needs to be called in the upper-most superclass of Controller with child
        stop_token.getUpdatedRef().clear();
        _thread.join();
    };
    //multiple inheritance: Overriding PassThru not ReaderImpl
    void event_loop(){
        int counter = 0;
        bool blink = true;
        while(stop_token.getUpdatedRef().test_and_set()){
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
        stop_token.getAcknowledgeRef().clear();
    }
    private:
    std::thread _thread;
};
}