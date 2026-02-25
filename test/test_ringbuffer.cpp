#include <iostream>
#include "RingBuffer.cpp"
using BLINK_T=std::array<RING_BUFFER::value_type,9>;
#include "software-on-silicon/ringbuffer_helpers.hpp"

class Functor {
    public:
    Functor() {}
    void operator()(BLINK_T& piece){
        for (std::size_t i=0; i<std::tuple_size<RING_BUFFER>{}-1; i++)
            WriteInterleaved<decltype(hostmemory),BLINK_T::value_type>(bus,piece[i]);
    }
    private:
    RING_BUFFER hostmemory = RING_BUFFER{};
    RingBufferBus<RING_BUFFER> bus{hostmemory.begin(),hostmemory.end()};
    RingBufferImpl buffer{bus};
};

using namespace std::chrono;

int main(){
    //const SAMPLE_TYPE channel_first[9]={0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0};
    //const SAMPLE_TYPE* channel_ptrs[1] = {channel_first};
    //const SAMPLE_TYPE** channelBuffers32 = static_cast<const SAMPLE_TYPE**>(channel_ptrs);
    //notconst Sample32(=float) **   channelBuffers32
    //std::array<SOS::MemoryView::sample<SAMPLE_TYPE,1>,MAX_BLINK> sample{{{0}}};
    RING_BUFFER::value_type sample{{{0}}};
    BLINK_T channelBuffers32{RING_BUFFER::value_type{{{0}}},RING_BUFFER::value_type{{{1}}},RING_BUFFER::value_type{{{2}}},RING_BUFFER::value_type{{{3}}},RING_BUFFER::value_type{{{4}}},RING_BUFFER::value_type{{{5}}},RING_BUFFER::value_type{{{6}}},RING_BUFFER::value_type{{{7}}},RING_BUFFER::value_type{{{8}}}};
    //BLINK_T channelBuffers32{{{{0}}},{{{1}}},{{{2}}},{{{3}}},{{{4}}},{{{5}}},{{{6}}},{{{7}}},{{{8}}}};
    Functor functor{};
    auto loopstart = high_resolution_clock::now();
    auto beginning = loopstart;
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        if ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - beginning).count() > 0)) {
        beginning = high_resolution_clock::now();
        functor(channelBuffers32);
        }
        std::this_thread::yield();
    }
    std::cout<<std::endl;
}