void recording_loop_interleaved(snd_pcm_t *handle, MEMORY_CONTROLLER &audio_data, std::size_t total_frames) {
    const snd_pcm_channel_area_t* areas = nullptr;
    snd_pcm_uframes_t offset = 0;

    std::size_t frames_read = 0;
    snd_pcm_uframes_t avail = 0;
    snd_pcm_uframes_t frames = 0;

    while (frames_read < total_frames) {
        avail = snd_pcm_avail(handle);
        if (avail < MAX_READ) {
            if (avail < 0) {
                rc(avail);
                snd_pcm_recover(handle, avail, 0);
            }
            rc(snd_pcm_wait(handle, -1));//only plughw
            //std::this_thread::sleep_for(std::chrono::milliseconds{39});//37 to 38ms
            continue;
        }
        snd_pcm_uframes_t chunk = MAX_BLINK;
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
        frames_read += MAX_BLINK;
    }
}

void recording_loop_noninterleaved(snd_pcm_t *handle, MEMORY_CONTROLLER &audio_data, std::size_t total_frames) {
    const snd_pcm_channel_area_t* areas = nullptr;
    snd_pcm_uframes_t offset = 0;

    std::size_t frames_read = 0;
    snd_pcm_uframes_t avail = 0;
    snd_pcm_uframes_t frames = 0;

    while (frames_read < total_frames) {
        avail = snd_pcm_avail_update(handle);//not hardware synced
        //avail = snd_pcm_avail(handle);
        if (avail < MAX_READ) {
            if (avail < 0) {
                rc(avail);
                snd_pcm_recover(handle, avail, 0);
            }
            rc(snd_pcm_wait(handle, -1));//only plughw
            //std::this_thread::sleep_for(std::chrono::milliseconds{39});//37 to 38ms
            continue;
        }
        snd_pcm_uframes_t chunk = MAX_BLINK;
        while (chunk>0) {
            frames = MAX_READ;
            rc(snd_pcm_mmap_begin(handle, &areas, &offset, &frames));
            if (frames<=chunk){
            if (areas){
                const auto channel_config = check_noninterleaved(areas);
                for (std::size_t j=0; j<NUM_CHANNELS;j++){// BUG?
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
        frames_read += MAX_BLINK;
    }
}