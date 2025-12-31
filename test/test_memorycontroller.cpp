/*

Not used in implementation of ARAFallback. This is only a preliminary refactoring check to get ARAFallback RingToMemory working.

*/

#include "MemoryController.cpp"

namespace SOSFloat {
class Functor {
    public:
    Functor(const std::size_t& vst_numInputs) : readerBus(vst_numInputs), controller(readerBus,vst_numInputs) {}
    void setReadBuffer(SOS::MemoryView::reader_traits<MEMORY_CONTROLLER>::input_container_type* buffer){
        randomread=buffer;
        readerBus.setReadBuffer(*randomread);
    }
    void setMemoryControllerOffset(const std::size_t offset) {
        readerBus.setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear()
    }
    void triggerReadStart(){
        readerBus.signal.getUpdatedRef().clear();
    }
    void operator()(const std::size_t offset){
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            setMemoryControllerOffset(offset);
            triggerReadStart();
        }
    }
    private:
    typename SOS::MemoryView::reader_traits<SOSFloat::MEMORY_CONTROLLER>::input_container_type* randomread;
    ReaderBus<typename SOS::MemoryView::reader_traits<SOSFloat::MEMORY_CONTROLLER>::input_container_type> readerBus;
    WritePriorityImpl controller;
};
}

using namespace std::chrono;

int main(){
    //API: NOTCONST void* const* buffers: target
    SOSFloat::SAMPLE_SIZE** buffers = nullptr;
    const std::size_t _ara_channelCount = 5;
    buffers = new SOSFloat::SAMPLE_SIZE*[_ara_channelCount];//OLD: buffers = (void**)malloc(surroundsound*sizeof(void*));
    //size_t Speczilla::ARAAudioSource::read(void * buffers[], size_t offset, size_t samplesPerChannel)
    const std::size_t ara_samplesPerChannel = 1000;
    for (std::size_t i=0;i<_ara_channelCount;i++){
        buffers[i]=new SOSFloat::SAMPLE_SIZE[ara_samplesPerChannel];
        for (std::size_t channel = 0; channel < ara_samplesPerChannel; channel++) {
            buffers[i][channel] = 0.0;
        }
    }
    const std::size_t ara_offset=7991;
    SOS::MemoryView::reader_traits<SOSFloat::MEMORY_CONTROLLER>::input_container_type randomread{};
    for (int i=0;i<_ara_channelCount;i++){
        randomread.push_back(SOS::MemoryView::ARAChannel(buffers[i],ara_samplesPerChannel));
    }
    std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
    auto functor = SOSFloat::Functor(_ara_channelCount);
    std::cout << "Reader reading 1000 times at position "<<ara_offset<<" of memory at rate 1/s..." << std::endl;
    functor.setReadBuffer(&randomread);
    functor.setMemoryControllerOffset(ara_offset);
    functor.triggerReadStart();
    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<12){
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