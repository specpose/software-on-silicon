//arecord -D hw:0,0 --dump-hw-params
//amixer -D hw:0
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <tuple>
#include <array>

const unsigned int rate = SAMPLE_RATE;

namespace SOS {
namespace Audio {
namespace Linux {
template<typename T> T rc(T err){
    if (err < 0) {
        fprintf(stderr, "ASOUND ERROR: %s\n", snd_strerror(err));
    }
    return err;
}

std::tuple<snd_pcm_t*,snd_pcm_hw_params_t*> init(unsigned int rate, snd_pcm_uframes_t* frames, snd_pcm_access_t format, bool hw = true){
    snd_pcm_t *handle;
    if (hw)
        rc(snd_pcm_open(&handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0));
    else
        rc(snd_pcm_open(&handle, "plughw:0,0", SND_PCM_STREAM_CAPTURE, 0));
    snd_pcm_hw_params_t *params;
    rc(snd_pcm_hw_params_malloc(&params));
    rc(snd_pcm_hw_params_any(handle, params));
    rc(snd_pcm_hw_params_set_access(handle, params, format));
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
    if (hw) {
    snd_pcm_uframes_t notification_interval = rate/(*frames);
    rc(snd_pcm_hw_params_set_period_size_near(handle, params, &notification_interval, &dir));
    if (dir != 0){
        fprintf(stderr, "PROGRAM ERROR: dir: %d, period_size forced to %d\n", dir, notification_interval);
        abort();
    }
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
    return std::tuple<pollfd*,int>{ufds, fd_count};
}

void start_pcm(snd_pcm_t *handle){
    snd_pcm_state_t state = rc(snd_pcm_state(handle));
    if (state!=SND_PCM_STATE_RUNNING)
        if (SND_PCM_STATE_RUNNING)
            rc(snd_pcm_start(handle));
    else{
        fprintf(stderr,"ASOUND ERROR: %d\n", state);
        abort();
    }
}

void destroy_poll(std::tuple<pollfd*,int> poll){
    for (int i=0;i<std::get<1>(poll);i++){
        close(std::get<0>(poll)[i].fd);
        std::get<0>(poll)[i].fd = -1;
    }
    if (std::get<0>(poll))
        free(std::get<0>(poll));
}

std::array<std::tuple<SAMPLE_TYPE*,std::size_t>,NUM_CHANNELS> check_interleaved(const snd_pcm_channel_area_t* areas){
    std::array<std::tuple<SAMPLE_TYPE*,std::size_t>,NUM_CHANNELS> channel_config{{{nullptr,0}}};
    for (std::size_t i = 0; i < NUM_CHANNELS; i++) {
        if (areas[i].first != i * sizeof(SAMPLE_TYPE) * 8){
            fprintf(stderr,"PROGRAM ERROR: channel offset of %d bits is not according to interleaved mode.\n", areas[i].first);
            abort();
        }
        if ((areas[i].step / (NUM_CHANNELS*sizeof(SAMPLE_TYPE)*8) ) != 1){
            fprintf(stderr,"PROGRAM ERROR: step size of %d bits is not for interleaved mode.\n", areas[i].step);
            abort();
        }
        channel_config[i] = std::tuple<SAMPLE_TYPE*,std::size_t>{
            (SAMPLE_TYPE*)areas[i].addr+i,
            NUM_CHANNELS
        };
    }
    return channel_config;
}

std::array<std::tuple<SAMPLE_TYPE*,std::size_t>,NUM_CHANNELS> check_noninterleaved(const snd_pcm_channel_area_t* areas){
    std::array<std::tuple<SAMPLE_TYPE*,std::size_t>,NUM_CHANNELS> channel_config{{{nullptr,0}}};
    const auto previous = areas[0].first;
    for (std::size_t i = 0; i < NUM_CHANNELS; i++) {
        if (areas[i].first % (sizeof(SAMPLE_TYPE)*8) != 0 | areas[i].first != previous){
            fprintf(stderr,"PROGRAM ERROR: channel offset of %d bits is not according to noninterleaved mode.\n", areas[i].first);
            abort();
        }
        if ((areas[i].step / (sizeof(SAMPLE_TYPE)*8) ) != 1){
            fprintf(stderr,"PROGRAM ERROR: step size of %d bits is not for noninterleaved mode.\n", areas[i].step);
            abort();
        }
        channel_config[i] = std::tuple<SAMPLE_TYPE*,std::size_t>{
            (SAMPLE_TYPE*)areas[i].addr+areas[i].first/(sizeof(SAMPLE_TYPE)*8),
            1
        };
    }
    return channel_config;
}

template<typename BlinkType, std::size_t Size=std::tuple_size<BlinkType>{}> void record_blink_rwinterleaved(BlinkType& audio_data, snd_pcm_t* handle, std::size_t& frames_read) {
    snd_pcm_uframes_t chunk = Size;
    snd_pcm_sframes_t read = 0;
    while (chunk>0) {
        read = rc(snd_pcm_readi(handle, &audio_data, MAX_READ));
        if (read==MAX_READ) {
            chunk -= MAX_READ;
        } else {
            fprintf(stderr, "PROGRAM ERROR: read %d", read);
            abort();
        }
    }
    frames_read += Size;
}

template<typename BlinkType, std::size_t Size=std::tuple_size<BlinkType>{}> void record_blink_poll(BlinkType& audio_data, snd_pcm_t* handle, std::size_t& frames_read, pollfd* ufds, unsigned int fd_count, snd_pcm_uframes_t& max) {
    snd_pcm_uframes_t avail = 0;
    unsigned short revents;

    snd_pcm_uframes_t chunk = Size;
    const snd_pcm_channel_area_t* areas = nullptr;
    snd_pcm_uframes_t offset = 0;
    snd_pcm_uframes_t frames = 0;
    auto block_offset = frames_read % Size;
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
    frames_read += Size;
}

bool check_avail(snd_pcm_t *handle) {
    snd_pcm_uframes_t avail = 0;
    avail = snd_pcm_avail(handle);
    if (avail < MAX_READ) {
        if (avail < 0) {
            rc(avail);
            snd_pcm_recover(handle, avail, 0);
        }
        rc(snd_pcm_wait(handle, -1));//only plughw
        //std::this_thread::sleep_for(std::chrono::milliseconds{39});//37 to 38ms
        return false;
    }
    return true;
}

template<typename BlinkType, std::size_t Size=std::tuple_size<BlinkType>{}> void record_blink_mmapinterleaved(BlinkType& audio_data, snd_pcm_t* handle, std::size_t& frames_read) {
    if (check_avail(handle)){
        snd_pcm_uframes_t chunk = Size;
        const snd_pcm_channel_area_t* areas = nullptr;
        snd_pcm_uframes_t offset = 0;
        snd_pcm_uframes_t frames = 0;
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
        frames_read += Size;
    }
}

template<typename BlinkType, std::size_t Size=std::tuple_size<BlinkType>{}> void record_blink_mmapnoninterleaved(BlinkType& audio_data, snd_pcm_t* handle, std::size_t& frames_read) {
    if (check_avail(handle)){
        snd_pcm_uframes_t chunk = Size;
        const snd_pcm_channel_area_t* areas = nullptr;
        snd_pcm_uframes_t offset = 0;
        snd_pcm_uframes_t frames = 0;
        while (chunk>0) {
            frames = MAX_READ;
            rc(snd_pcm_mmap_begin(handle, &areas, &offset, &frames));
            if (frames<=chunk){
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
                if (auto read = rc(snd_pcm_mmap_commit(handle, offset, frames))!=frames )
                    rc(read >= 0 ? -EPIPE : read);
                chunk -= frames;
            } else {
                fprintf(stderr, "PROGRAM ERROR: read %d", frames);
                abort();
            }
        }
        frames_read += Size;
    }
}
}}}