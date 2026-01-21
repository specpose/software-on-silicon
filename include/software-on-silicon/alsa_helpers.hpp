//arecord -D hw:0,0 --dump-hw-params
//amixer -D hw:0
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <tuple>
#include <array>

#define INTEL 0
#if INTEL
#define MAX_READ 32 //<8192
#else
#define MAX_READ 8 //<8192
#endif

template<typename T> T rc(T err){
    if (err < 0) {
        fprintf(stderr, "ASOUND ERROR: %s\n", snd_strerror(err));
    }
    return err;
}

std::tuple<snd_pcm_t*,snd_pcm_hw_params_t*> init(unsigned int rate, snd_pcm_uframes_t* frames){
    snd_pcm_t *handle;
    rc(snd_pcm_open(&handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0));
    snd_pcm_hw_params_t *params;
    rc(snd_pcm_hw_params_malloc(&params));
    rc(snd_pcm_hw_params_any(handle, params));
    rc(snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_MMAP_INTERLEAVED));
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
}

void destroy(std::tuple<snd_pcm_t*,snd_pcm_hw_params_t*> driver){
    rc(snd_pcm_drop(std::get<0>(driver)));
    snd_pcm_hw_params_free(std::get<1>(driver));
    std::get<1>(driver) = nullptr;
    rc(snd_pcm_close(std::get<0>(driver)));
}

std::tuple<pollfd*,unsigned int> init_poll(snd_pcm_t *handle){
    unsigned int fd_count = 0;
    fd_count = snd_pcm_poll_descriptors_count(handle);
    if (fd_count <= 0) {
        fprintf(stderr, "Invalid poll descriptors count\n");
    }
    pollfd* ufds = (pollfd*)malloc(sizeof(pollfd) * fd_count);
    rc(snd_pcm_poll_descriptors(handle, ufds, fd_count));
    rc(snd_pcm_start(handle));
    return std::tuple<pollfd*,int>{ufds, fd_count};
}

void destroy_poll(std::tuple<pollfd*,int> poll){
    for (int i=0;i<std::get<1>(poll);i++){
        close(std::get<0>(poll)[i].fd);
        std::get<0>(poll)[i].fd = -1;
    }
    if (std::get<0>(poll))
        free(std::get<0>(poll));
}

std::array<std::tuple<SAMPLE_SIZE*,std::size_t>,NUM_CHANNELS> check_interleaved(const snd_pcm_channel_area_t* areas){
    std::array<std::tuple<SAMPLE_SIZE*,std::size_t>,NUM_CHANNELS> channel_config{{{nullptr,0}}};
    for (std::size_t i = 0; i < NUM_CHANNELS; i++) {
        if (areas[i].first != i * sizeof(SAMPLE_SIZE) * 8){
            fprintf(stderr,"PROGRAM ERROR: channel offset of %d bits is not according to interleaved mode.\n", areas[i].first);
            abort();
        }
        if ((areas[i].step / (NUM_CHANNELS*sizeof(SAMPLE_SIZE)*8) ) != 1){
            fprintf(stderr,"PROGRAM ERROR: step size of %d bits is not for interleaved mode.\n", areas[i].step);
            abort();
        }
        channel_config[i] = std::tuple<SAMPLE_SIZE*,std::size_t>{
            (SAMPLE_SIZE*)areas[i].addr+i,
            NUM_CHANNELS
        };
    }
    return channel_config;
}