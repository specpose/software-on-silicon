#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <chrono>

using namespace SOS;

#include <iostream>
namespace SOSFloat {
using SAMPLE_SIZE = float;
#include "software-on-silicon/arafallback_helpers.hpp"
using RING_BUFFER = std::vector<std::tuple<unsigned int,std::vector<SAMPLE_SIZE>, unsigned int>>;
using MEMORY_CONTROLLER = std::vector<SAMPLE_SIZE>;
using READ_BUFFER = std::vector<SAMPLE_SIZE>;

class ReadTaskImpl : public SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    ReadTaskImpl(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) :
    SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER>(Length, Offset, blockercable)
    {}
    protected:
    void read(){
        //readbuffer
        const auto start = _size.getReadBufferStartRef().load();
        auto current = start;
        const auto end = _size.getReadBufferAfterLastRef().load();
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
                if (std::distance(start,current)>=std::distance(readerStart+readOffset,readerEnd))
                    *current = 0.0;
                else
                    *current = *(readerPos++);
                ++current;
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
class WriteTaskImpl : public SOS::Behavior::WriteTask<std::tuple_element<1,RING_BUFFER::value_type>::type> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<std::tuple_element<1,RING_BUFFER::value_type>::type>() {
        resize(0);
    }
    virtual void resize(typename std::tuple_element<1,RING_BUFFER::value_type>::type::difference_type newsize){
        memorycontroller.reserve(newsize);
        while(memorycontroller.size()<newsize)
            memorycontroller.push_back(0.0);
        std::get<0>(_blocker.cables).getBKStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef().store(memorycontroller.end());
    };
    //helper function, not inherited
    void write(const RING_BUFFER::value_type character) {
        resize(std::get<0>(character)+std::get<2>(character));
        if (std::distance(std::get<0>(_blocker.cables).getBKStartRef().load(),std::get<0>(_blocker.cables).getBKEndRef().load())<
        std::get<2>(character)+std::get<0>(character))
            throw SFA::util::runtime_error("Writer tried to write beyond memorycontroller bounds",__FILE__,__func__);
        writerPos = std::get<0>(_blocker.cables).getBKStartRef().load() + std::get<2>(character);
        for(int i=0;i<std::get<0>(character);i++)
            SOS::Behavior::WriteTask<std::tuple_element<1,RING_BUFFER::value_type>::type>::write(std::get<1>(character)[i]);
    }
    //not inherited
    void clearMemoryController() {
        memorycontroller.clear();
        memorycontroller.resize(0);
    }
};
class TransferRingToMemory : protected Behavior::RingBufferTask<RING_BUFFER>, protected WriteTaskImpl {
    public:
    TransferRingToMemory(
        Behavior::RingBufferTask<RING_BUFFER>::cable_type& indices,
        Behavior::RingBufferTask<RING_BUFFER>::const_cable_type& bounds
        ) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds), WriteTaskImpl{} {}
    protected:
    //multiple inheritance: ambiguous override!
    virtual void write(const RING_BUFFER::value_type character) final {
        _blocker.signal.getNotifyRef().clear();
        WriteTaskImpl::write(character);
        _blocker.signal.getNotifyRef().test_and_set();
    }
};
class RingBufferImpl : public TransferRingToMemory, protected SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>> {
    public:
    RingBufferImpl(MemoryView::RingBufferBus<RING_BUFFER>& rB,MemoryView::ReaderBus<READ_BUFFER>& rd) :
    TransferRingToMemory(std::get<0>(rB.cables),std::get<0>(rB.const_cables)),
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

//Helper classes
class Functor1 {
    public:
    Functor1(MemoryView::ReaderBus<READ_BUFFER>& readerBus, const unsigned int maxSamplesPerProcess, bool start=false) :
    _readerBus(readerBus),
    _maxSamplesPerProcess(maxSamplesPerProcess),
    test_AudioBuffer(new SAMPLE_SIZE[maxSamplesPerProcess]),
    hostmemory(RING_BUFFER(2,std::tuple(0,std::vector<SAMPLE_SIZE>(maxSamplesPerProcess),0)))
    {
        if (start)
            _thread = std::thread{std::mem_fn(&Functor1::test_write_loop),this};
    }
    ~Functor1(){
        _thread.join();
        if (test_AudioBuffer!=nullptr)
            delete test_AudioBuffer;
    }
    void operator()(SAMPLE_SIZE* buffer,const unsigned int actualProcessSize, const int actualSamplePosition){
        if (actualProcessSize>_maxSamplesPerProcess)
            throw SFA::util::logic_error("Writer can only write maxSamplesPerProcess",__FILE__,__func__);
        
        hostwriter.write(buffer,actualSamplePosition,actualProcessSize);//lock free write
    }
    void test_write_loop(){
        unsigned int _actualProcessSize=333;
        auto loopstart = high_resolution_clock::now();
        //try {
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
            const auto beginning = high_resolution_clock::now();
            switch(test_count++){
                case 0:
                    fill(test_AudioBuffer,_actualProcessSize,1.0);
                    test_count++;
                    break;
                case 1:
                    fill(test_AudioBuffer,_actualProcessSize,0.0);
                    test_count++;
                    break;
                case 2:
                    fill(test_AudioBuffer,_actualProcessSize,0.0);
                    test_count=0;
                    break;
            }
            operator()(test_AudioBuffer,_actualProcessSize,test_actualSamplePosition);
            test_actualSamplePosition += _actualProcessSize;
            std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{333}));
        }
        //} catch (std::exception& e) {
        //std::cout<<std::endl<<"RingBuffer Shutdown"<<std::endl;
        //}
    }
    private:
    void fill(SAMPLE_SIZE buffer[],const unsigned int actualProcessSize, const SAMPLE_SIZE value){
        for(int i=0;i<actualProcessSize;i++)
            buffer[i]=value;
    }
    MemoryView::ReaderBus<READ_BUFFER>& _readerBus;

    RING_BUFFER hostmemory;
    MemoryView::RingBufferBus<RING_BUFFER> ringbufferbus{hostmemory.begin(),hostmemory.end()};
    //if RingBufferImpl<ReaderImpl> shuts down too early, Piecewriter is catching up
    //=>Piecewriter needs readerimpl running
    RingBufferImpl buffer{ringbufferbus,_readerBus};

    PieceWriter<decltype(hostmemory)> hostwriter{ringbufferbus};//not a thread!
    unsigned int test_count = 0;
    unsigned int test_actualSamplePosition = 0;
    unsigned int _maxSamplesPerProcess;
    //not strictly necessary, simulate real-world use-scenario
    std::thread _thread = std::thread{};
    //error: flexible array member ‘Functor1::AudioBuffer’ not at end of ‘class Functor1’
    SAMPLE_SIZE* test_AudioBuffer = nullptr;//Sample32=float **   channelBuffers32
};
class Functor2 {
    public:
    Functor2(bool start=false, int readOffset=0) : test_readOffset(readOffset) {
        randomread.reserve(1000);
        while(randomread.size()<1000)
            randomread.push_back(0.0);
        readerBus.setReadBuffer(randomread);
        if (start) {
            readerBus.setOffset(test_readOffset);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
            _thread = std::thread{std::mem_fn(&Functor2::test_read_loop),this};
        }
    }
    ~Functor2(){
        _thread.join();
    }
    bool operator()(READ_BUFFER& buffer, const unsigned int readOffset){
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
        readerBus.setReadBuffer(buffer);
        readerBus.setOffset(readOffset);//FIFO has to be called before each getUpdatedRef().clear()
        readerBus.signal.getUpdatedRef().clear();
        return true;
        }
        return false;
    }
    void test_read_loop() {
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<11) {
        const auto beginning = high_resolution_clock::now();
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.setOffset(test_readOffset);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
            std::cout<<"Reading offset "<<test_readOffset<<":"<<std::endl;
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << *print++;
            std::cout << std::endl;
        }
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1000}));
        }
    }
    MemoryView::ReaderBus<READ_BUFFER> readerBus{};
    private:
    //size_t Speczilla::ARAAudioSource::read(void * buffers[], size_t offset, size_t samplesPerChannel)
    //or std::vector<T> storage = std::vector<T>(channelCount*samplesPerChannel, 0.0f);
    READ_BUFFER randomread = READ_BUFFER{};
    
    unsigned int test_readOffset = 0;
    //not strictly necessary, simulate real-world use-scenario
    std::thread _thread = std::thread{};
};
}
int main (){
    const int offset = 9990-667;
    std::cout << "Reader reading 1000 characters per second at position " << offset << "..." << std::endl;
    //read
    auto functor2 = SOSFloat::Functor2(true, offset);
    std::cout << "Writer writing 9990 times (10s) from start at rate 1/ms..." << std::endl;
    //write
    auto functor1 = SOSFloat::Functor1(functor2.readerBus, 334, true);
}