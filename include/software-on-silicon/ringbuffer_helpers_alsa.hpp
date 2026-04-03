template<typename Current, typename Handle, typename Count> void write_blink_interleaved(Current& current, Handle *handle, Count& frames_read){
    SOS::Audio::Linux::record_blink_rwinterleaved(current, handle, frames_read);
}
template<typename Current, typename Handle, typename... Args> void write_blink_interleaved(Current& current, Handle handle, Args... args){
    SOS::Audio::Linux::record_blink_poll(current, handle, args...);
}