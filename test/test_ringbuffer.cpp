#include <iostream>
#include "RingBuffer.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"
#include <chrono>

namespace SOSFloat {
class Functor {
    public:
    Functor(const std::size_t numInputs) : vst_numInputs(numInputs) {
        //hostmemory.fill(new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(5));
        for(auto& sample : hostmemory)
            sample=new SOS::MemoryView::Contiguous<SAMPLE_SIZE>(vst_numInputs);
    }
    ~Functor() {
        for (auto& sample : hostmemory)
            if (sample)
                delete sample;
    }
    void operator()(const SAMPLE_SIZE* channel_ptrs[], const std::size_t vst_numSamples){
        PieceWriter<decltype(hostmemory)>(bus,channel_ptrs,vst_numInputs, vst_numSamples);
    }
    private:
    RING_BUFFER hostmemory = RING_BUFFER{};
    RingBufferBus<RING_BUFFER> bus{hostmemory.begin(),hostmemory.end()};
    RingBufferImpl buffer{bus};
    std::size_t vst_numInputs;//vst numInputs
};
}

using namespace std::chrono;

int main(){
    std::size_t numInputs = 5;//vst numInputs
    auto functor = SOSFloat::Functor(numInputs);
    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto beginning = high_resolution_clock::now();
        //from source
        const SOSFloat::SAMPLE_SIZE channel_even[9]={0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0};
        const SOSFloat::SAMPLE_SIZE channel_odd[9]={0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9};
        const SOSFloat::SAMPLE_SIZE channel_last[9]={1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0,1.0};
        const SOSFloat::SAMPLE_SIZE* channel_ptrs[5] = {channel_even,channel_odd,channel_even,channel_odd,channel_last};//vst host assume const
        const SOSFloat::SAMPLE_SIZE** channelBuffers32 = static_cast<const SOSFloat::SAMPLE_SIZE**>(channel_ptrs);//notconst Sample32(=float) **   channelBuffers32
        std::size_t numSamples = 9;//vst numSamples
        std::size_t actualSamplePosition = 0;//vst actualSamplePosition
        functor(channelBuffers32,numSamples);
        //deallocating source not needed: Owned by vst
        //error: free(): invalid pointer
        //for (size_t i=0;i<numInputs;i++)
        //    delete channelBuffers32[i];
        //error: free(): invalid size
        //delete channelBuffers32;
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
    }
    std::cout<<std::endl;
}