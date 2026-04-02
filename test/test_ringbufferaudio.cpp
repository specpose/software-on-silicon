#include <iostream>

#define INTEL 1
#if INTEL
#define MAX_BLINK 2
#define MAX_READ 2
#else
#define MAX_BLINK 4
#define MAX_READ 4
#endif
#define SAMPLE_TYPE short
#include "RingBuffer.cpp"
#include "software-on-silicon/alsa_helpers.hpp"
#include "software-on-silicon/alsa_ringbuffer.hpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

class Functor {
    public:
    Functor() {}
    void operator()(snd_pcm_t *handle){
        std::size_t frames_read = 0;
        while (frames_read + MAX_READ <= MAX_BLINK)
            WriteInterleaved(bus, handle, frames_read);
    }
    private:
    RING_BUFFER hostmemory = RING_BUFFER{};
    SOS::MemoryView::RingBufferBus<RING_BUFFER> bus{hostmemory.begin(),hostmemory.end()};
    RingBufferImpl buffer{bus};
};

using namespace std::chrono;

int main(){
    Functor functor{};
    snd_pcm_uframes_t period_size = MAX_READ;

    auto driver = init(rate, &period_size, SND_PCM_ACCESS_RW_INTERLEAVED, true);
    start_pcm(std::get<0>(driver));
    auto start = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-start).count()<10) {
        functor(std::get<0>(driver));
        std::this_thread::yield();
    }
    destroy(driver);
}