#define SAMPLE_TYPE short
#include "software-on-silicon/alsa_helpers.hpp"
#include <chrono>
#include <thread>
#include <vector>

#if INTEL
#define MAX_BLINK 2
#define MAX_READ 2
#define STORAGE_SIZE 480000
#else
#define MAX_BLINK 4
#define MAX_READ 4
#define STORAGE_SIZE 80000
#endif

using RING_BUFFER = std::array<std::array<std::array<SAMPLE_TYPE,NUM_CHANNELS>,MAX_READ>,10>;
#include "software-on-silicon/alsa_ringbuffer.hpp"

int main(){
    static_assert(MAX_BLINK%MAX_READ==0);
    static_assert(STORAGE_SIZE%MAX_READ==0);
    const int seconds = 10;
    assert((rate*seconds)%STORAGE_SIZE==0);
    RING_BUFFER buffer{};
    const RING_BUFFER::value_type::value_type sample{{0x00,0x00}};
    for (std::size_t j=0;j<std::tuple_size<RING_BUFFER>{}; j++){
        for (std::size_t i=0;i<MAX_READ;i++){
            buffer[j][i]=sample;
        }
    }
    std::size_t ringbuffer_index = 0;
    snd_pcm_uframes_t period_size = MAX_READ;

    auto driver = init(rate, &period_size, SND_PCM_ACCESS_RW_INTERLEAVED, true);
    start_pcm(std::get<0>(driver));
    std::size_t frames_read = 0;
    auto start = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<10) {
        record_blink(std::get<0>(driver), buffer[ringbuffer_index], frames_read, STORAGE_SIZE);
        for (std::size_t i=0; i<MAX_READ; i++){
            for (std::size_t j=0; j<NUM_CHANNELS; j++){
                fprintf(stdout, ",%04d", buffer[ringbuffer_index][i][j]);
            }
        }
        ringbuffer_index = ringbuffer_index == 9 ? 0 : ++ringbuffer_index;
        std::this_thread::yield();
    }
    destroy(driver);
    fprintf(stdout, "\n");
}
