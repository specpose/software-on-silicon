template<typename Target, typename InputBlock> void WriteInterleaved(SOS::MemoryView::RingBufferBus<Target>& myBus, InputBlock& buffer){
    auto current = std::get<0>(myBus.cables).getCurrentRef().load();
    const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
    const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            std::cout<<"=";
            //write directly to HOSTmemory
            *current=buffer;
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            //write last bit
            *current=buffer;
            //current invalid => do not advance
            std::cout<<std::endl;
            SFA::util::runtime_error(SFA::util::error_code::RingbufferTooSlowOrNotBigEnough,__FILE__,__func__);
        }
}