#include "RingToMemory.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

namespace SOSFloat {
//Helper classes
class Functor1 {
    public:
    Functor1(MemoryView::ReaderBus<READ_BUFFER>& readerBus, const std::size_t& numInputs, const std::size_t maxSamplesPerProcess, bool start=false) :
    _vst_numInputs(numInputs), vst_maxSamplesPerProcess(maxSamplesPerProcess),
    _readerBus(readerBus), buffer(RingBufferImpl{ringbufferbus,_readerBus,numInputs}),
    ara_sampleCount(buffer.ara_sampleCount) {
        for(std::size_t ring_entry=0;ring_entry<hostmemory.size();ring_entry++){
            std::get<0>(hostmemory[ring_entry]) = new SOS::MemoryView::Contiguous<SAMPLE_SIZE>*[vst_maxSamplesPerProcess];
            for(std::size_t sample=0;sample<vst_maxSamplesPerProcess;sample++)
                std::get<0>(hostmemory[ring_entry])[sample]= new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(_vst_numInputs);
        }
        if (start)
            startTestLoop();
    }
    ~Functor1(){
        _thread.join();
        for(std::size_t ring_entry=0;ring_entry<hostmemory.size();ring_entry++){
            for(std::size_t sample=0;sample<vst_maxSamplesPerProcess;sample++)
                delete std::get<0>(hostmemory[ring_entry])[sample];
            delete std::get<0>(hostmemory[ring_entry]);
        }
    }
    void operator()(const SAMPLE_SIZE* channel_ptrs[], const std::size_t vst_numSamples, const std::size_t actualSamplePosition){
        if (!channel_ptrs)
            throw SFA::util::logic_error("Supplied VST buffer not initialised",__FILE__,__func__);
        for (std::size_t channel=0;channel<_vst_numInputs;channel++)
            if (!channel_ptrs[channel])
                throw SFA::util::logic_error("Supplied VST buffer channels not initialised",__FILE__,__func__);
        PieceWriterRtM<decltype(hostmemory)>(ringbufferbus,channel_ptrs,_vst_numInputs, vst_numSamples, actualSamplePosition);
    }
    void startTestLoop(){
        _thread = std::thread{std::mem_fn(&Functor1::test_loop),this};
    }
    void reset() {//only call after last Piecewriter.write
        buffer.resetAndRestart();
    }
    void test_loop(){
        std::size_t actualSamplePosition = 0;
        const std::size_t numSamples = 333;//vst numSample
        auto loopstart = high_resolution_clock::now();
        //try {
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<9) {
            const auto beginning = high_resolution_clock::now();
            SOSFloat::SAMPLE_SIZE blink[333]={};//vst AudioEffect: avoid heap allocation and locks
            switch(count){
                case 0:
                    std::fill(&(blink[0]),&(blink[332]),1.0);
                    count++;
                    break;
                case 1:
                    std::fill(&(blink[0]),&(blink[332]),0.0);
                    count++;
                    break;
                case 2:
                    std::fill(&(blink[0]),&(blink[332]),0.0);
                    count=0;
                    break;
            }
            const SOSFloat::SAMPLE_SIZE* channel_ptrs[5] = {blink,blink,blink,blink,blink};
            const SOSFloat::SAMPLE_SIZE** channelBuffers32 = static_cast<const SOSFloat::SAMPLE_SIZE**>(channel_ptrs);//notconst Sample32(=float) **   channelBuffers32
            operator()(channelBuffers32,numSamples,actualSamplePosition);//lock free write
            actualSamplePosition += 333;//vst actualSamplePosition
            //deallocating source not needed: Owned by vst
            //error: free(): invalid pointer
            //for (size_t i=0;i<vst_numInputs;i++)
            //    delete channelBuffers32[i];
            //    delete channel_ptrs[i];
            //error: free(): invalid size
            //delete channelBuffers32;
            //delete channel_ptrs;
            //delete blink;
            std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{333}));
        }
        //} catch (std::exception& e) {
        //std::cout<<std::endl<<"RingBuffer Shutdown"<<std::endl;
        //}
    }
    const MEMORY_CONTROLLER::difference_type& ara_sampleCount;
    private:
    MemoryView::ReaderBus<READ_BUFFER>& _readerBus;

    RING_BUFFER hostmemory = RING_BUFFER{};
    MemoryView::RingBufferBus<RING_BUFFER> ringbufferbus{hostmemory.begin(),hostmemory.end()};
    //if RingBufferImpl<ReaderImpl> shuts down too early, Piecewriter is catching up
    //=>Piecewriter needs readerimpl running
    RingBufferImpl buffer;

    unsigned int count = 0;
    const std::size_t& _vst_numInputs;//vst numInputs
    const std::size_t vst_maxSamplesPerProcess;//vst maxSamplesPerProcess
    //not strictly necessary, simulate real-world use-scenario
    std::thread _thread;
};
class Functor2 {
    public:
    Functor2(const std::size_t vst_numInputs) : vst_numChannels(vst_numInputs), readerBus(vst_numInputs) {}
    ~Functor2(){
        wipeBufferProxy();
    };
    void setReadBuffer(SOSFloat::SAMPLE_SIZE** buffers,const std::size_t ara_samplesPerChannel){
        if (randomread)
            throw SFA::util::logic_error("operator()() call has not finished",__FILE__,__func__);
        if (!buffers)
            throw SFA::util::logic_error("Supplied ARA buffer not initialised",__FILE__,__func__);
        for (std::size_t channel=0;channel<vst_numChannels;channel++)
            if (!buffers[channel])
                throw SFA::util::logic_error("Supplied ARA buffer channels not initialised",__FILE__,__func__);
        randomread = new SOSFloat::READ_BUFFER{};
        for (int i=0;i<vst_numChannels;i++){
            randomread->push_back(SOS::MemoryView::ARAChannel(buffers[i],ara_samplesPerChannel));
        }
        //randomread=buffer;
        readerBus.setReadBuffer(*randomread);
    }
    void setMemoryControllerOffset(const std::size_t offset) {
        readerBus.setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear()
    }
    void triggerReadStart(){
        if (trigger)
            throw SFA::util::logic_error("FIFO read call already in progress",__FILE__,__func__);
        else
            trigger = true;
        if (!randomread)
            throw SFA::util::logic_error("No ReadBuffer supplied",__FILE__,__func__);
        readerBus.signal.getUpdatedRef().clear();
    }
    bool operator()() {
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            wipeBufferProxy();
            trigger = false;
            return true;
        } else {
            return false;
        }
    }
    MemoryView::ReaderBus<READ_BUFFER> readerBus;
    const std::size_t vst_numChannels;
    private:
    void wipeBufferProxy(){
        if (randomread)
            delete randomread;
        randomread=nullptr;
    }
    READ_BUFFER* randomread = nullptr;
    std::size_t _ara_samplesPerChannel = 0;
    bool trigger = false;
};
}

using namespace std::chrono;

int main (){
    const std::size_t _vst_maxSamplesPerChannel=500;//333 perProcess
    const std::size_t _vst_numInputs = 5;
    const std::size_t ara_offset=2996;
    std::cout << "Reader reading 1000 characters per second at position " << ara_offset << "..." << std::endl;
    //read
    auto functor2 = SOSFloat::Functor2(_vst_numInputs);
    std::cout << "Writer writing 9990 times (10s) from start at rate 1/ms..." << std::endl;
    //write
    auto functor1 = SOSFloat::Functor1(functor2.readerBus,functor2.vst_numChannels, _vst_maxSamplesPerChannel, false);//GCC bug: true is auto-converted to std::size_t if _vst_numInputs missing!!

    //API: NOTCONST void* const* buffers: target
    SOSFloat::SAMPLE_SIZE** buffers = nullptr;
    buffers = new SOSFloat::SAMPLE_SIZE*[functor2.vst_numChannels];//OLD: buffers = (void**)malloc(surroundsound*sizeof(void*));
    //size_t Speczilla::ARAAudioSource::read(void * buffers[], size_t offset, size_t samplesPerChannel)
    const std::size_t ara_samplesPerChannel = 1000;
    for (std::size_t channel=0;channel<functor2.vst_numChannels;channel++){
        buffers[channel]=new SOSFloat::SAMPLE_SIZE[ara_samplesPerChannel];
    }

    functor1.startTestLoop();
    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<5) {
        const auto beginning = high_resolution_clock::now();
        functor2.setReadBuffer(buffers, ara_samplesPerChannel);
        functor2.setMemoryControllerOffset(ara_offset);
        functor2.triggerReadStart();
        while(!functor2())
            std::this_thread::yield();
        auto print = &buffers[4][0];//HACK: hard-coded channel 5
        while (print!=&buffers[4][ara_samplesPerChannel])//HACK: hard-coded channel 5
            std::cout << *(print++);
        std::cout << std::endl;
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1000}));
    }

    //deallocating target: Needed ARAFallback is a host
    if (buffers){
        for (std::size_t i=0;i<functor2.vst_numChannels;i++)
            delete buffers[i];
        delete buffers;
    }
}