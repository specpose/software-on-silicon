template<typename Target, typename InputBlock> void WriteInterleaved(SOS::MemoryView::RingBufferBus<Target>& myBus, InputBlock& buffer){
    auto current = std::get<0>(myBus.cables).getCurrentRef().load();
    const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
    const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
    std::cout<<"=";
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
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
//has different for{for{}}
template<typename Target, typename InputBlock> void WriteInterleaved(SOS::MemoryView::RingBufferBus<Target>& myBus, InputBlock& buffer, const std::size_t position){//puts absolutePosition into Ringbuffer
    auto current = std::get<0>(myBus.cables).getCurrentRef().load();
    const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
    const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
    if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
        //write directly to HOSTmemory
        std::get<1>(*current)=position;
        //for (std::size_t sample=0;sample<std::tuple_size<InputBlock>{};sample++){
        //    (std::get<0>(*current)[sample])=buffer[sample];
        //}
        std::get<0>(*current)=buffer;
        ++current;
        if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
            current = std::get<0>(myBus.const_cables).getWriterStartRef();
        std::get<0>(myBus.cables).getCurrentRef().store(current);
        myBus.signal.getNotifyRef().clear();
    } else {
        //write last bit
        std::get<1>(*current)=position;
        //for (std::size_t sample=0;sample<std::tuple_size<InputBlock>{};sample++){
        //    (std::get<0>(*current)[sample])=buffer[sample];
        //}
        std::get<0>(*current)=buffer;
        //do not advance -> current invalid
        std::cout<<std::endl;
        SFA::util::runtime_error(SFA::util::error_code::RingbufferTooSlowOrNotBigEnough,__FILE__,__func__);
    }
}