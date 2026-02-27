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
    BLINK_T channelBuffers32{};
    //initialise channel of sample to its channel number
    for (std::size_t i = 0; i < std::size(channelBuffers32); i++)
        for (std::size_t j = 0; j < std::size(channelBuffers32[i]); j++)
            channelBuffers32[i][j].channels[0] = BLINK_T::value_type::value_type::sample_type(i);
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