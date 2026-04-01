void record_blink(RING_BUFFER::value_type &audio_data, snd_pcm_t *handle, std::size_t& frames_read) {
    snd_pcm_sframes_t read = 0;
    read = rc(snd_pcm_readi(handle, &audio_data, MAX_READ));
    if (read==MAX_READ) {
        frames_read += MAX_READ;
    } else {
        fprintf(stderr, "PROGRAM ERROR: read %d", read);
        abort();
    }
}

void record_block(RING_BUFFER::value_type &audio_data, snd_pcm_t *handle, std::size_t& frames_read, pollfd* ufds, unsigned int fd_count, snd_pcm_uframes_t& max) {
    const snd_pcm_channel_area_t* areas = nullptr;
    snd_pcm_uframes_t offset = 0;

    snd_pcm_uframes_t avail = 0;
    snd_pcm_uframes_t frames = 0;

    unsigned short revents;
    auto start = std::chrono::high_resolution_clock::now();

    snd_pcm_uframes_t chunk = MAX_BLINK;
    auto block_offset = frames_read % MAX_BLINK;
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
    frames_read += MAX_BLINK;
}