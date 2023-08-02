#include "MemoryController.cpp"

namespace SOSFloat {
class Functor {
    public:
    Functor(const std::size_t vst_numInputs) : vst_numInputs(vst_numInputs), readerBus(vst_numInputs), controller(readerBus) {}
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
    void operator()(int offset){
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            setOffset(offset);
            triggerReadStart();
        }
    }
    private:
    READ_BUFFER* randomread;
    ReaderBus<READ_BUFFER> readerBus;
    WritePriorityImpl controller;
    const std::size_t vst_numInputs;
};
}

int main(){
    //API: NOTCONST void* const* buffers: target
    SOSFloat::SAMPLE_SIZE** buffers = nullptr;
    const std::size_t _ara_channelCount = 5;
    buffers = new SOSFloat::SAMPLE_SIZE*[_ara_channelCount];//OLD: buffers = (void**)malloc(surroundsound*sizeof(void*));
    //size_t Speczilla::ARAAudioSource::read(void * buffers[], size_t offset, size_t samplesPerChannel)
    const std::size_t ara_samplesPerChannel = 1000;
    for (std::size_t i=0;i<_ara_channelCount;i++){
        buffers[i]=new SOSFloat::SAMPLE_SIZE[ara_samplesPerChannel];
    }
    const std::size_t ara_offset=7991;
    auto randomread = SOSFloat::READ_BUFFER{};
    for (int i=0;i<_ara_channelCount;i++){
        randomread.push_back(SOS::MemoryView::ARAChannel(buffers[i],ara_samplesPerChannel));
    }
    std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
    auto functor = SOSFloat::Functor(_ara_channelCount);
    std::cout << "Reader reading 1000 times at position "<<ara_offset<<" of memory at rate 1/s..." << std::endl;
    functor.setReadBuffer(&randomread);
    functor.setOffset(ara_offset);
    functor.triggerReadStart();
    while (true){
        const auto start_tp = high_resolution_clock::now();
        functor(ara_offset);
        for (std::size_t i=0;i<ara_samplesPerChannel;i++)
            std::cout<< buffers[4][i];
        std::cout<<std::endl;
        std::this_thread::sleep_until(start_tp + duration_cast<high_resolution_clock::duration>(seconds{1}));
    }
    //deallocating target: Needed ARAFallback is a host
    if (buffers){
        for (std::size_t i=0;i<_ara_channelCount;i++)
            delete buffers[i];
        delete buffers;
    }
}