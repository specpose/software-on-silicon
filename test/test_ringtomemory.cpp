#include "RingToMemory.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"
#include <chrono>

namespace SOSFloat {
//Helper classes
/*class Functor1 {
    public:
    Functor1(MemoryView::ReaderBus<READ_BUFFER>& readerBus, bool start=false) : _readerBus(readerBus){
        if (start)
            _thread = std::thread{std::mem_fn(&Functor1::operator()),this};
    }
    ~Functor1(){
        _thread.join();
    }
    void operator()(){
        auto loopstart = high_resolution_clock::now();
        //try {
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
            const auto beginning = high_resolution_clock::now();
            switch(count++){
                case 0:
                    hostwriter('*', 333);//lock free write
                    break;
                case 1:
                    hostwriter('_', 333);
                    break;
                case 2:
                    hostwriter('_', 333);
                    count=0;
                    break;
            }
            std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{333}));
        }
        //} catch (std::exception& e) {
        //std::cout<<std::endl<<"RingBuffer Shutdown"<<std::endl;
        //}
    }
    private:
    MemoryView::ReaderBus<READ_BUFFER>& _readerBus;

    RING_BUFFER hostmemory = RING_BUFFER{};
    MemoryView::RingBufferBus<RING_BUFFER> ringbufferbus{hostmemory.begin(),hostmemory.end()};
    //if RingBufferImpl<ReaderImpl> shuts down too early, Piecewriter is catching up
    //=>Piecewriter needs readerimpl running
    RingBufferImpl buffer{ringbufferbus,_readerBus};

    PieceWriter<decltype(hostmemory)> hostwriter{ringbufferbus};//not a thread!
    unsigned int count = 0;
    //not strictly necessary, simulate real-world use-scenario
    std::thread _thread = std::thread{};
};*/
class Functor2 {
    public:
    Functor2(const std::size_t vst_numInputs) : vst_numInputs(vst_numInputs), readerBus(vst_numInputs) {}
    void setReadBuffer(READ_BUFFER* buffer){
        randomread=buffer;
        readerBus.setReadBuffer(*randomread);
    }
    void setOffset(const std::size_t offset) {
        readerBus.setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear()
    }
    void triggerReadStart(){
        readerBus.signal.getUpdatedRef().clear();
    }
    void operator()(const std::size_t offset) {
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear()
            triggerReadStart();
        }
    }
    MemoryView::ReaderBus<READ_BUFFER> readerBus;
    private:
    READ_BUFFER* randomread;
    const std::size_t vst_numInputs;
};
}

using namespace std::chrono;

int main (){
    const std::size_t _ara_channelCount = 5;
    //std::cout << "Writer writing 9990 times (10s) from start at rate 1/ms..." << std::endl;
    //write
    //auto functor1 = SOSFloat::Functor1(functor2.readerBus, true);
    const std::size_t ara_offset=2996;
    std::cout << "Reader reading 1000 characters per second at position " << ara_offset << "..." << std::endl;
    //read
    auto functor2 = SOSFloat::Functor2(_ara_channelCount);

    //API: NOTCONST void* const* buffers: target
    SOSFloat::SAMPLE_SIZE** buffers = nullptr;
    buffers = new SOSFloat::SAMPLE_SIZE*[_ara_channelCount];//OLD: buffers = (void**)malloc(surroundsound*sizeof(void*));
    //size_t Speczilla::ARAAudioSource::read(void * buffers[], size_t offset, size_t samplesPerChannel)
    const std::size_t ara_samplesPerChannel = 1000;
    for (std::size_t i=0;i<_ara_channelCount;i++){
        buffers[i]=new SOSFloat::SAMPLE_SIZE[ara_samplesPerChannel];
    }
    auto randomread = SOSFloat::READ_BUFFER{};
    for (int i=0;i<_ara_channelCount;i++){
        randomread.push_back(SOS::MemoryView::ARAChannel(buffers[i],ara_samplesPerChannel));
    }
    functor2.setReadBuffer(&randomread);
    functor2.setOffset(ara_offset);
    functor2.triggerReadStart();

    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<5) {
        const auto beginning = high_resolution_clock::now();
        auto print = randomread[4].begin();//HACK: hard-coded channel 5
        while (print!=randomread[4].end())//HACK: hard-coded channel 5
            std::cout << (*print)++;
        std::cout << std::endl;
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1000}));
    }
}