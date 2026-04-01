template<typename Target, typename InputBlock> void WriteInterleaved(SOS::MemoryView::RingBufferBus<Target>& myBus, const InputBlock& buffer){
    auto current = std::get<0>(myBus.cables).getCurrentRef().load();
    const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
    const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
    //std::cout<<"=";
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            //write directly to HOSTmemory
            write_blink_interleaved(current, buffer);
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            //write last bit
            write_blink_interleaved(current, buffer);
            //current invalid => do not advance
            std::cout<<std::endl;
            SFA::util::runtime_error(SFA::util::error_code::RingbufferTooSlowOrNotBigEnough,__FILE__,__func__);
        }
}