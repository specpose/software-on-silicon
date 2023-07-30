#include "RingToMemory.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"
#include <chrono>

namespace SOSFloat {
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