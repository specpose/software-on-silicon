using SAMPLE_SIZE = short;
#define NUM_CHANNELS 2
#include "software-on-silicon/alsa_helpers.hpp"
#include <chrono>
#include <thread>
#include <vector>
//#include <poll.h>

#if INTEL
#define BLOCK_SIZE 48000
#else
#define BLOCK_SIZE 8000
#endif

using RING_BUFFER = std::vector<std::array<std::array<SAMPLE_SIZE,NUM_CHANNELS>,BLOCK_SIZE>>;

void record_block(snd_pcm_t *handle, RING_BUFFER::value_type &audio_data, snd_pcm_uframes_t& offset, pollfd* ufds, unsigned int fd_count, snd_pcm_uframes_t& max) {
    const snd_pcm_channel_area_t* areas = nullptr;

    snd_pcm_uframes_t avail = 0;
    snd_pcm_uframes_t frames = 0;
    snd_pcm_uframes_t read = 0;

    unsigned short revents;
    auto start = std::chrono::high_resolution_clock::now();

    snd_pcm_uframes_t chunk = BLOCK_SIZE;
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
                if (areas){
                    const auto block_offset = offset % BLOCK_SIZE;
                    const auto channel_config = check_interleaved(areas);
                    for (std::size_t j=0; j<NUM_CHANNELS;j++){
                        for (std::size_t i=0;i<MAX_READ;i++) {
                            audio_data[block_offset+i][j]=*(std::get<0>(channel_config[j]) +
                            i *
                            std::get<1>(channel_config[j]));
                            //fprintf(stdout, ",%04d", audio_data[block_offset+i][j]);
                        }
                    }
                } else {
                    fprintf(stderr, "PROGRAM ERROR: invalid mmap areas\n");
                    abort();
                }
                read = rc(snd_pcm_mmap_commit(handle, offset, MAX_READ));
                if (read < 0) {
                    fprintf(stderr, "PROGRAM ERROR: mmap read count is %d\n", read);
                    abort();
                    read = 0;
                } else if (read != MAX_READ){
                    fprintf(stderr, "PROGRAM ERROR: frames dropped %d\n", MAX_READ - read);
                    abort();
                }
                chunk -= read;
            }
            max = max < avail ? avail : max;
        }
        std::this_thread::yield();
    }
}

int main(){
    static_assert(BLOCK_SIZE%MAX_READ==0);
#if INTEL
    const unsigned int rate = 48000;
#else
    const unsigned int rate = 8000;
#endif
    assert((NUM_CHANNELS*rate)%BLOCK_SIZE==0);
    auto buffer = RING_BUFFER{};
    buffer.push_back(RING_BUFFER::value_type{});
    buffer.push_back(RING_BUFFER::value_type{});
    const RING_BUFFER::value_type::value_type sample{{0xFE,0xFE}};
    for (std::size_t j=0;j<buffer.size();j++)
        for (std::size_t i=0;i<BLOCK_SIZE;i++){
            buffer[j][i]=sample;
        }
    std::size_t ringbuffer_index = 0;
    snd_pcm_uframes_t period_size = MAX_READ;//NUM_CHANNELS * 27;//notification interval

    snd_pcm_uframes_t offset = 0;
    snd_pcm_uframes_t max = 0;

    auto driver = init(rate, &period_size);
    auto poll = init_poll(std::get<0>(driver));
    auto start = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<10){
        record_block(std::get<0>(driver), buffer[ringbuffer_index], offset, std::get<0>(poll), std::get<1>(poll), max);
        ringbuffer_index = ringbuffer_index == 0 ? 1 : 0;
    }
    destroy(driver);
    destroy_poll(poll);
    for(std::size_t i=0;i<BLOCK_SIZE;i++)
        for (std::size_t j=0;j<NUM_CHANNELS;j++)
            fprintf(stdout, ",%04d", buffer[ringbuffer_index == 0 ? 1 : 0][i][j]);
    fprintf(stdout, "\n");
    fprintf(stdout,"Maximum read backlog %d\n",max);
}
