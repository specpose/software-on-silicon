#include "RingBuffer.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

class Functor {
    public:
    Functor() {}
    void operator()(){
        const auto piece = RING_BUFFER::value_type{{'+'}};//HACK: hard coded channel 0
        for (std::size_t i=0;i<std::tuple_size<RING_BUFFER>{}-1;i++)
            WriteInterleaved<decltype(hostmemory)>(bus,piece);
    }
    private:
    RING_BUFFER hostmemory = RING_BUFFER{};
    RingBufferBus<RING_BUFFER> bus{hostmemory.begin(),hostmemory.end()};
    RingBufferImpl buffer{bus};
};

using namespace std::chrono;

int main(){
    Functor functor{};
    auto loopstart = high_resolution_clock::now();
    auto beginning = loopstart;
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        if ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - beginning).count() > 0)) {
        beginning = high_resolution_clock::now();
        functor();
        }
        std::this_thread::yield();
    }
    std::cout<<std::endl;
}