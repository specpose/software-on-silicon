#include <chrono>
#include <thread>
#include <vector>
#include "software-on-silicon/alsa_helpers.hpp"

using RING_BUFFER = std::array<std::array<std::array<SAMPLE_TYPE,NUM_CHANNELS>,MAX_BLINK>,10>;

using namespace SOS::Audio::Linux;

int main(){
    static_assert(MAX_BLINK%MAX_READ==0);
    static_assert(STORAGE_SIZE%MAX_BLINK==0);
    const int seconds = TOTAL_TIME;
    assert((rate*seconds)/STORAGE_SIZE==1);
    RING_BUFFER buffer{};
    const RING_BUFFER::value_type::value_type sample{{0x00,0x00}};
    for (std::size_t j=0;j<std::tuple_size<RING_BUFFER>{}; j++){
        for (std::size_t i=0;i<MAX_BLINK;i++){
            buffer[j][i]=sample;
        }
    }
    std::size_t ringbuffer_index = 0;
    snd_pcm_uframes_t period_size = MAX_READ;

    auto driver = init(rate, &period_size, SND_PCM_ACCESS_RW_INTERLEAVED, true);
    start_pcm(std::get<0>(driver));
    std::size_t frames_read = 0;
    auto start = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<12) {
        record_blink_rwinterleaved<RING_BUFFER::value_type>(buffer[ringbuffer_index], std::get<0>(driver), frames_read);
        for (std::size_t i=0; i<MAX_BLINK; i++){
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
