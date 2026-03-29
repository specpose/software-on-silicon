#define SAMPLE_TYPE short
#include "software-on-silicon/alsa_helpers.hpp"
#include <chrono>
#include <thread>
#include <vector>

#if INTEL
#define MAX_READ 32 //<8192
#define MAX_BLINK 1
#else
#define MAX_READ 8 //<8192
#define MAX_BLINK 1
#endif
#if INTEL
#define STORAGE_SIZE 480000
#else
#define STORAGE_SIZE 80000
#endif

using MEMORY_CONTROLLER = std::array<std::array<SAMPLE_TYPE,NUM_CHANNELS>,STORAGE_SIZE>;
#include "software-on-silicon/alsa_memorycontroller.hpp"

int main(){
    static_assert(STORAGE_SIZE%(MAX_BLINK*MAX_READ)==0);
    const int seconds = 10;
    assert((NUM_CHANNELS*rate*seconds)%STORAGE_SIZE==0);
    MEMORY_CONTROLLER buffer{};
    const MEMORY_CONTROLLER::value_type sample{{0x00,0x00}};
    for (std::size_t i=0;i<STORAGE_SIZE;i++){
        buffer[i]=sample;
    }
    snd_pcm_uframes_t period_size = MAX_READ;

    auto driver = init(rate, &period_size, SND_PCM_ACCESS_MMAP_INTERLEAVED, true);
    start_pcm(std::get<0>(driver));
    recording_loop_interleaved(std::get<0>(driver), buffer, seconds * rate);
    destroy(driver);
    for(std::size_t i=0;i<seconds * rate;i++)
        for (std::size_t j=0;j<NUM_CHANNELS;j++)
            fprintf(stdout, ",%04d", buffer[i][j]);
    fprintf(stdout, "\n");
}
