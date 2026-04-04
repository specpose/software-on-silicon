#include <chrono>
#include <thread>
#include <vector>
#include "software-on-silicon/alsa_helpers.hpp"

using BLINK_T=std::array<std::array<SAMPLE_TYPE,NUM_CHANNELS>,MAX_BLINK>;
using MEMORY_CONTROLLER = std::array<std::array<SAMPLE_TYPE,NUM_CHANNELS>,STORAGE_SIZE>;

using namespace SOS::Audio::Linux;

#include <iostream>
int main(){
    static_assert(MAX_BLINK%MAX_READ==0);
    static_assert(STORAGE_SIZE%MAX_BLINK==0);
    const int seconds = TOTAL_TIME;
    assert((rate*seconds)/STORAGE_SIZE==1);
    MEMORY_CONTROLLER buffer{};
    const MEMORY_CONTROLLER::value_type sample{{0x00,0x00}};
    for (std::size_t i=0;i<STORAGE_SIZE;i++){
        buffer[i]=sample;
    }
    snd_pcm_uframes_t period_size = MAX_READ;

    auto driver = init(rate, &period_size, SND_PCM_ACCESS_MMAP_INTERLEAVED, true);
    start_pcm(std::get<0>(driver));
    std::size_t frames_read = 0;
    while (frames_read + MAX_BLINK <= STORAGE_SIZE) {
        BLINK_T& pos = reinterpret_cast<BLINK_T&>(buffer[frames_read]);
        record_blink_mmapinterleaved<BLINK_T,MAX_BLINK>(pos, std::get<0>(driver), frames_read);
    }
    destroy(driver);
    for(std::size_t i=0;i<seconds * rate;i++)
        for (std::size_t j=0;j<NUM_CHANNELS;j++)
            fprintf(stdout, ",%04d", buffer[i][j]);
    fprintf(stdout, "\n");
}
