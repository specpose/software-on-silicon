//arecord -D hw:0,0 --dump-hw-params
//amixer -D hw:0
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <tuple>
#include <chrono>
#include <thread>
#include <vector>
#include <array>

#define NUM_CHANNELS 2
#define MAX_READ 8
#define INTEL 0
#if INTEL
#define BLOCK_SIZE 480000
#else
#define BLOCK_SIZE 80000
#endif

template<typename T> T rc(T err){
    if (err < 0) {
        fprintf(stderr, "ASOUND ERROR: %s\n", snd_strerror(err));
    }
    return err;
}

std::tuple<snd_pcm_t*,snd_pcm_hw_params_t*> init(unsigned int rate, snd_pcm_uframes_t* frames){
    int dir = 0;
    snd_pcm_t *handle;
    rc(snd_pcm_open(&handle, "plughw:0,0", SND_PCM_STREAM_CAPTURE, 0));
    //rc(snd_pcm_open(&handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0));
    snd_pcm_hw_params_t *params;
    rc(snd_pcm_hw_params_malloc(&params));
    rc(snd_pcm_hw_params_any(handle, params));
    rc(snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_MMAP_NONINTERLEAVED));
    rc(snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE));
    assert(snd_pcm_format_big_endian(SND_PCM_FORMAT_S16_LE)!=1);
    rc(snd_pcm_hw_params_set_channels(handle, params, NUM_CHANNELS));
    rc(snd_pcm_hw_params_set_rate_near(handle, params, &rate, &dir));
    if (dir != 0){
        fprintf(stderr, "PROGRAM ERROR: rate %d not supported, dir: %d\n", rate, dir);
        abort();
    }
    rc(snd_pcm_hw_params(handle, params));
    return std::tuple<snd_pcm_t*,snd_pcm_hw_params_t*>{handle, params};
}

void destroy(std::tuple<snd_pcm_t*,snd_pcm_hw_params_t*> driver){
    rc(snd_pcm_drop(std::get<0>(driver)));
    snd_pcm_hw_params_free(std::get<1>(driver));
    std::get<1>(driver) = nullptr;
    rc(snd_pcm_close(std::get<0>(driver)));
}

using SAMPLE_SIZE = short;

std::array<std::tuple<SAMPLE_SIZE*,std::size_t>,NUM_CHANNELS> check_noninterleaved(const snd_pcm_channel_area_t* areas){
    std::array<std::tuple<SAMPLE_SIZE*,std::size_t>,NUM_CHANNELS> channel_config{{{nullptr,0}}};
    const auto previous = areas[0].first;
    for (std::size_t i = 0; i < NUM_CHANNELS; i++) {
        if (areas[i].first % (sizeof(SAMPLE_SIZE)*8) != 0 | areas[i].first != previous){
            fprintf(stderr,"PROGRAM ERROR: channel offset of %d bits is not according to noninterleaved mode.\n", areas[i].first);
            abort();
        }
        if ((areas[i].step / (sizeof(SAMPLE_SIZE)*8) ) != 1){
            fprintf(stderr,"PROGRAM ERROR: step size of %d bits is not for noninterleaved mode.\n", areas[i].step);
            abort();
        }
        channel_config[i] = std::tuple<SAMPLE_SIZE*,std::size_t>{
            (SAMPLE_SIZE*)areas[i].addr+areas[i].first/(sizeof(SAMPLE_SIZE)*8),
            1
        };
    }
    return channel_config;
}

using RING_BUFFER = std::vector<std::array<std::array<SAMPLE_SIZE,NUM_CHANNELS>,BLOCK_SIZE>>;

void recording_loop(snd_pcm_t *handle, RING_BUFFER::value_type &audio_data, std::size_t total_frames) {
    const snd_pcm_channel_area_t* areas = nullptr;
    snd_pcm_uframes_t offset = 0;

    std::size_t frames_read = 0;
    snd_pcm_state_t state;
    snd_pcm_uframes_t avail = 0;
    snd_pcm_uframes_t frames = 0;

    bool first = true;
    while (frames_read < total_frames) {
        state = rc(snd_pcm_state(handle));
        if (state!=SND_PCM_STATE_RUNNING)
            if (SND_PCM_STATE_RUNNING)
                rc(snd_pcm_start(handle));
        else{
            fprintf(stderr,"ASOUND ERROR: %d\n", state);
            abort();
        }
        // 1. Update the local view of available buffer space
        avail = snd_pcm_avail_update(handle);//not hardware synced
        //avail = snd_pcm_avail(handle);
        if (avail < MAX_READ) {
            if (avail < 0) {
                // 2. Handle errors like -EPIPE (underrun)
                rc(avail);
                snd_pcm_recover(handle, avail, 0); // Automatic recovery
            }
            rc(snd_pcm_wait(handle, -1));//only plughw
            //std::this_thread::sleep_for(std::chrono::milliseconds{39});//37 to 38ms
            continue;
        }
        snd_pcm_uframes_t chunk = MAX_READ;
        while (chunk>0) {
            frames = chunk;
            rc(snd_pcm_mmap_begin(handle, &areas, &offset, &frames));
            if (areas){
                const auto channel_config = check_noninterleaved(areas);
                for (std::size_t j=0; j<NUM_CHANNELS;j++){
                    for (std::size_t i=0;i<(frames);i++) {
                        audio_data[frames_read+i][j]=*(std::get<0>(channel_config[j]) +
                        i *
                        std::get<1>(channel_config[j]));
                        //fprintf(stdout, ",%04d", audio_data[frames_read+i][j]);
                    }
                }
            }
            if (auto commitres = rc(snd_pcm_mmap_commit(handle, offset, frames))!=frames )
                rc(commitres >= 0 ? -EPIPE : commitres);
            chunk -= frames;
        }
        frames_read += MAX_READ;
    }
}

int main(){
    static_assert(BLOCK_SIZE%MAX_READ==0);
#if INTEL
    const unsigned int rate = 48000;
#else
    const unsigned int rate = 8000;
#endif
    const int seconds = 10;
    assert((NUM_CHANNELS*rate*seconds)%BLOCK_SIZE==0);
    auto buffer = RING_BUFFER{};
    buffer.push_back(RING_BUFFER::value_type{});
    const RING_BUFFER::value_type::value_type sample{{0x00,0x00}};
    for (std::size_t i=0;i<BLOCK_SIZE;i++){
        buffer[0][i]=sample;
    }
    snd_pcm_uframes_t period_size = NUM_CHANNELS * 27;//notification interval

    auto driver = init(rate, &period_size);
    recording_loop(std::get<0>(driver), buffer[0], seconds * rate);
    destroy(driver);
    for(std::size_t i=0;i<seconds * rate;i++)
        for (std::size_t j=0;j<NUM_CHANNELS;j++)
            fprintf(stdout, ",%04d", buffer[0][i][j]);
    fprintf(stdout, "\n");
}
