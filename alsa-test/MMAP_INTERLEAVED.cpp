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
#define BLOCK_SIZE 480000
#else
#define BLOCK_SIZE 80000
#endif

using RING_BUFFER = std::vector<std::array<std::array<SAMPLE_TYPE,NUM_CHANNELS>,BLOCK_SIZE>>;

void recording_loop(snd_pcm_t *handle, RING_BUFFER::value_type &audio_data, std::size_t total_frames) {
    const snd_pcm_channel_area_t* areas = nullptr;
    snd_pcm_uframes_t offset = 0;

    std::size_t frames_read = 0;
    snd_pcm_uframes_t avail = 0;
    snd_pcm_uframes_t frames = 0;

    while (frames_read < total_frames) {
        avail = snd_pcm_avail(handle);
        if (avail < MAX_READ) {
            if (avail < 0) {
                // 2. Handle errors like -EPIPE (underrun)
                rc(avail);
                snd_pcm_recover(handle, avail, 0);
            }
            rc(snd_pcm_wait(handle, -1));//only plughw
            //std::this_thread::sleep_for(std::chrono::milliseconds{39});//37 to 38ms
            continue;
        }
        snd_pcm_uframes_t chunk = MAX_BLINK * MAX_READ;
        while (chunk>0) {
            frames = MAX_READ;
            rc(snd_pcm_mmap_begin(handle, &areas, &offset, &frames));
            if (frames<=chunk){
            if (areas){
                const auto channel_config = check_interleaved(areas);
                for (std::size_t j=0; j<NUM_CHANNELS;j++){
                    for (std::size_t i=0;i<(frames);i++) {
                        audio_data[frames_read+i][j]=*(std::get<0>(channel_config[j]) +
                        i *
                        std::get<1>(channel_config[j]));
                        //fprintf(stdout, ",%04d", audio_data[frames_read+i][j]);
                    }
                }
            }
            if (auto read = rc(snd_pcm_mmap_commit(handle, offset, frames))!=frames )
                rc(read >= 0 ? -EPIPE : read);
            chunk -= frames;
            } else {
                fprintf(stderr, "PROGRAM ERROR: read %d", frames);
                abort();
            }
        }
        frames_read += MAX_BLINK * MAX_READ;
    }
}

int main(){
    static_assert(BLOCK_SIZE%(MAX_BLINK*MAX_READ)==0);
    const int seconds = 10;
    assert((NUM_CHANNELS*rate*seconds)%BLOCK_SIZE==0);
    auto buffer = RING_BUFFER{};
    buffer.push_back(RING_BUFFER::value_type{});
    const RING_BUFFER::value_type::value_type sample{{0x00,0x00}};
    for (std::size_t i=0;i<BLOCK_SIZE;i++){
        buffer[0][i]=sample;
    }
    snd_pcm_uframes_t period_size = MAX_READ;

    auto driver = init(rate, &period_size, SND_PCM_ACCESS_MMAP_INTERLEAVED, true);
    start_pcm(std::get<0>(driver));
    recording_loop(std::get<0>(driver), buffer[0], seconds * rate);
    destroy(driver);
    for(std::size_t i=0;i<seconds * rate;i++)
        for (std::size_t j=0;j<NUM_CHANNELS;j++)
            fprintf(stdout, ",%04d", buffer[0][i][j]);
    fprintf(stdout, "\n");
}
