#include "RingBuffer.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"
#include <iostream>
#include <chrono>

class Functor {
    public:
    Functor() {}
    void operator()(){
        PieceWriter<decltype(hostmemory)>(bus,'+',32);
        //hostwriter('+',32);
    }
    private:
    RING_BUFFER hostmemory = RING_BUFFER{};
    RingBufferBus<RING_BUFFER> bus{hostmemory.begin(),hostmemory.end()};
    RingBufferImpl buffer{bus};
};

using namespace std::chrono;

int main(){
    auto functor = Functor();
    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto beginning = high_resolution_clock::now();
        functor();
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
    }
    std::cout<<std::endl;
}