#define SAMPLE_TYPE short
#include "software-on-silicon/alsa_helpers.hpp"
#include <vector>

#if INTEL
#define MAX_READ 2
#define BLOCK_SIZE 480000
#else
#define MAX_READ 4
#define BLOCK_SIZE 80000
#endif

/*std::tuple<snd_pcm_t*,snd_pcm_hw_params_t*> init(unsigned int rate, snd_pcm_uframes_t* frames){
    snd_pcm_t *handle;
    rc(snd_pcm_open(&handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0));
    snd_pcm_hw_params_t *params;
    rc(snd_pcm_hw_params_malloc(&params));
    rc(snd_pcm_hw_params_any(handle, params));
    rc(snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED));
    rc(snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE));
    assert(snd_pcm_format_big_endian(SND_PCM_FORMAT_S16_LE)!=1);
    rc(snd_pcm_hw_params_set_channels(handle, params, NUM_CHANNELS));
    int dir = 0;
    rc(snd_pcm_hw_params_set_rate_near(handle, params, &rate, &dir));
    if (dir != 0){
        fprintf(stderr, "PROGRAM ERROR: rate %d not supported, dir: %d\n", rate, dir);
        abort();
    }
#if INTEL
    snd_pcm_uframes_t notification_interval = rate/(*frames);
    rc(snd_pcm_hw_params_set_period_size_near(handle, params, &notification_interval, &dir));
    if (dir != 0){
        fprintf(stderr, "PROGRAM ERROR: dir: %d, period_size forced to %d\n", dir, notification_interval);
        abort();
    }
#endif
    rc(snd_pcm_hw_params(handle, params));
    return std::tuple<snd_pcm_t*,snd_pcm_hw_params_t*>{handle, params};
}*/

using RING_BUFFER = std::vector<std::array<std::array<SAMPLE_TYPE,NUM_CHANNELS>,BLOCK_SIZE>>;

void recording_loop(snd_pcm_t *handle, RING_BUFFER::value_type &audio_data, std::size_t total_frames) {
    std::size_t frames_read = 0;
    bool start = true;
    bool end = true;
    while (frames_read < total_frames) {
        snd_pcm_sframes_t read = 0;
        if (frames_read + MAX_READ <= total_frames){
        if (frames_read==0 && start){
            start = false;
            audio_data[frames_read][0]=0xFF;
            audio_data[frames_read][1]=0xFF;
            read = 2;
        }
        else if (frames_read==total_frames-MAX_READ && end){
            end = false;
            audio_data[frames_read+MAX_READ-1][0]=0xFF;
            audio_data[frames_read+MAX_READ-1][1]=0xFF;
            read = 2;
        }
        else {
            read = rc(snd_pcm_readi(handle, &audio_data[frames_read][0], MAX_READ));
        }
        if (read==MAX_READ) {
            frames_read += MAX_READ;
        } else {
            fprintf(stderr, "PROGRAM ERROR: read %d", read);
            abort();
        }
        }
    }
}

int main(){
    static_assert(BLOCK_SIZE%MAX_READ==0);
    const int seconds = 10;
    assert((rate*seconds)%BLOCK_SIZE==0);
    auto buffer = RING_BUFFER{};
    buffer.push_back(RING_BUFFER::value_type{});
    const RING_BUFFER::value_type::value_type sample{{0x00,0x00}};
    for (std::size_t i=0;i<BLOCK_SIZE;i++){
        buffer[0][i]=sample;
    }
    snd_pcm_uframes_t period_size = MAX_READ;

    auto driver = init(rate, &period_size, SND_PCM_ACCESS_RW_INTERLEAVED, true);
    recording_loop(std::get<0>(driver), buffer[0], BLOCK_SIZE);
    destroy(driver);
    for(std::size_t i=0;i<BLOCK_SIZE;i++)
        for (std::size_t j=0;j<NUM_CHANNELS;j++)
            fprintf(stdout, ",%04d", buffer[0][i][j]);
    fprintf(stdout, "\n");
}
