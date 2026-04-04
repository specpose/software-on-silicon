#include <iostream>
#include "RingBuffer.cpp"
#include "software-on-silicon/alsa_helpers.hpp"
#include "software-on-silicon/ringbuffer_helpers_alsa.hpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

class Functor {
    public:
    Functor() {}
    void operator()(snd_pcm_t *handle){
        std::size_t frames_read = 0;
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

    auto driver = SOS::Audio::Linux::init(rate, &period_size, SND_PCM_ACCESS_RW_INTERLEAVED, true);
    SOS::Audio::Linux::start_pcm(std::get<0>(driver));
    auto start = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-start).count()<10) {
        functor(std::get<0>(driver));
        std::this_thread::yield();
    }
    SOS::Audio::Linux::destroy(driver);
}