#define SAMPLE_TYPE short
#include "software-on-silicon/alsa_helpers.hpp"
#include <chrono>
#include <thread>
#include <vector>
//#include <poll.h>

#if INTEL
#define MAX_READ 32 //<8192
#else
#define MAX_READ 8 //<8192
#endif
#if INTEL
#define BLOCK_SIZE 48000
#else
#define BLOCK_SIZE 8000
#endif

using RING_BUFFER = std::vector<std::array<std::array<SAMPLE_TYPE,NUM_CHANNELS>,BLOCK_SIZE>>;

void record_block(snd_pcm_t *handle, RING_BUFFER::value_type &audio_data, std::size_t& frames_read, pollfd* ufds, unsigned int fd_count, snd_pcm_uframes_t& max) {
    const snd_pcm_channel_area_t* areas = nullptr;
    snd_pcm_uframes_t offset = 0;

    snd_pcm_uframes_t avail = 0;
    snd_pcm_uframes_t frames = 0;

    unsigned short revents;
    auto start = std::chrono::high_resolution_clock::now();

    snd_pcm_uframes_t chunk = BLOCK_SIZE;
    auto block_offset = frames_read % BLOCK_SIZE;
    while (chunk>0) {
        rc(poll(ufds, fd_count, -1));
        rc(snd_pcm_poll_descriptors_revents(handle, ufds, fd_count, &revents));
        if (revents & POLLERR){
            fprintf(stderr, "POLLERR\n");
            abort();
        }
        if (revents & POLLIN){
            auto avail = rc(snd_pcm_avail(handle));
            if (avail >= MAX_READ) {
                frames = MAX_READ;
                rc(snd_pcm_mmap_begin(handle, &areas, &offset, &frames));
                if (frames<=chunk) {
                if (areas){
                    const auto channel_config = check_interleaved(areas);
                    for (std::size_t j=0; j<NUM_CHANNELS;j++){
                        for (std::size_t i=0;i<frames;i++) {
                            audio_data[block_offset+i][j]=*(std::get<0>(channel_config[j]) +
                            i *
                            std::get<1>(channel_config[j]));
                            //fprintf(stdout, ",%04d", audio_data[block_offset+i][j]);
                        }
                    }
                    block_offset += frames;
                } else {
                    fprintf(stderr, "PROGRAM ERROR: invalid mmap areas\n");
                    abort();
                }
                snd_pcm_uframes_t read = rc(snd_pcm_mmap_commit(handle, offset, frames));
                if (read < 0) {
                    fprintf(stderr, "PROGRAM ERROR: mmap read count is %d\n", read);
                    abort();
                    read = 0;
                } else if (read != MAX_READ){
                    fprintf(stderr, "PROGRAM ERROR: frames dropped %d\n", MAX_READ - read);
                    abort();
                }
                chunk -= frames;
                } else {
                    fprintf(stderr, "PROGRAM ERROR: read %d", frames);
                    abort();
                }
            }
            max = max < avail ? avail : max;
        }
        std::this_thread::yield();
    }
    frames_read += BLOCK_SIZE;
}

int main(){
    static_assert(BLOCK_SIZE%MAX_READ==0);
    assert((NUM_CHANNELS*rate)%BLOCK_SIZE==0);
    auto buffer = RING_BUFFER{};
    const int seconds = 10;
    for (std::size_t i = 0; i < seconds; i++){
        buffer.push_back(RING_BUFFER::value_type{});
        const RING_BUFFER::value_type::value_type sample{{0xFE,0xFE}};
        for (std::size_t j=0;j<buffer.size();j++)
            for (std::size_t i=0;i<BLOCK_SIZE;i++){
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
        record_block(std::get<0>(driver), buffer[ringbuffer_index], frames_read, std::get<0>(poll), std::get<1>(poll), max);
        ringbuffer_index = ringbuffer_index == 9 ? 0 : ++ringbuffer_index;
    }
    destroy(driver);
    destroy_poll(poll);
    for (std::size_t k = 0; k < seconds; k++)
        for(std::size_t i=0;i<BLOCK_SIZE;i++)
            for (std::size_t j=0;j<NUM_CHANNELS;j++)
                fprintf(stdout, ",%04d", buffer[k][i][j]);
    fprintf(stdout, "\n");
    //fprintf(stdout,"Maximum read backlog %d\n",max);
}
