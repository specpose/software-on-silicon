#include <chrono>
#include <thread>
#include <vector>
//#include <poll.h>
#include "software-on-silicon/alsa_helpers.hpp"

using RING_BUFFER = std::vector<std::array<std::array<SAMPLE_TYPE,NUM_CHANNELS>,MAX_BLINK>>;

using namespace SOS::Audio::Linux;

int main(){
    static_assert(MAX_BLINK%MAX_READ==0);
    assert(rate%MAX_BLINK==0);
    auto buffer = RING_BUFFER{};
    const int seconds = TOTAL_TIME;
    for (std::size_t i = 0; i < seconds; i++){
        buffer.push_back(RING_BUFFER::value_type{});
        const RING_BUFFER::value_type::value_type sample{{0xFE,0xFE}};
        for (std::size_t j=0;j<buffer.size();j++)
            for (std::size_t i=0;i<(MAX_BLINK);i++){
                buffer[j][i]=sample;
            }
    }
    std::size_t ringbuffer_index = 0;
    snd_pcm_uframes_t period_size = MAX_READ;//NUM_CHANNELS * 27;//notification interval

    std::size_t frames_read = 0;
    snd_pcm_uframes_t max = 0;

    auto driver = init(rate, &period_size, SND_PCM_ACCESS_MMAP_INTERLEAVED, true);
    auto poll = init_poll(std::get<0>(driver));
    start_pcm(std::get<0>(driver));
    auto start = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<10){
        record_blink_poll<RING_BUFFER::value_type>(buffer[ringbuffer_index], std::get<0>(driver), frames_read, std::get<0>(poll), std::get<1>(poll), max);
        ringbuffer_index = ringbuffer_index == seconds-1 ? 0 : ++ringbuffer_index;
    }
    destroy(driver);
    destroy_poll(poll);
    for (std::size_t k = 0; k < seconds; k++)
        for(std::size_t i=0;i<(MAX_BLINK);i++)
            for (std::size_t j=0;j<NUM_CHANNELS;j++)
                fprintf(stdout, ",%04d", buffer[k][i][j]);
    fprintf(stdout, "\n");
    //fprintf(stdout,"Maximum read backlog %d\n",max);
}
