template <typename Current, typename... Args>
void write_blink_interleaved(Current current, Args... args)
{
    std::cout << "=";
};
template <typename Current, typename Buffer>
void write_blink_interleaved(Current& current, const Buffer& buffer)
{
    // std::cout<<"=";
    for (std::size_t sample = 0; sample < std::tuple_size<RING_BUFFER::value_type> {}; sample++)
        current[sample] = buffer[sample];
}
template <typename Target, typename... Args>
void WriteInterleaved(SOS::MemoryView::RingBufferBus<Target>& myBus, Args... args)
{
    auto current = std::get<0>(myBus.cables).getCurrentRef().load();
    const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
    const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
    if (current != std::get<0>(myBus.cables).getThreadCurrentRef().load()) {
        // write directly to HOSTmemory
        write_blink_interleaved(*current, args...);
        ++current;
        if (current == std::get<0>(myBus.const_cables).getWriterEndRef())
            current = std::get<0>(myBus.const_cables).getWriterStartRef();
        std::get<0>(myBus.cables).getCurrentRef().store(current);
        myBus.signal.getNotifyRef().clear();
    } else {
        // write last bit
        write_blink_interleaved(*current, args...);
        // current invalid => do not advance
        std::cout << std::endl;
        SFA::util::runtime_error(SFA::util::error_code::RingbufferTooSlowOrNotBigEnough, __FILE__, __func__);
    }
}