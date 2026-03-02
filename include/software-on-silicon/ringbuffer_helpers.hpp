template<typename Target, typename InputBlock> void WriteInterleaved(SOS::MemoryView::RingBufferBus<Target>& myBus, const InputBlock& buffer){
    auto current = std::get<0>(myBus.cables).getCurrentRef().load();
    const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
    const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
    //std::cout<<"=";
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            //write directly to HOSTmemory
            std::cout<<"write directly to HOSTmemory "<<std::tuple_size<InputBlock>{}<<std::endl;
            for (std::size_t sample=0;sample<std::tuple_size<InputBlock>{};sample++)
                (*current)[sample]=buffer[sample];
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            //write last bit
            std::cout<<"write directly to HOSTmemory "<<std::tuple_size<InputBlock>{}<<std::endl;
            for (std::size_t sample=0;sample<std::tuple_size<InputBlock>{};sample++)
                (*current)[sample]=buffer[sample];
            //current invalid => do not advance
            std::cout<<std::endl;
            SFA::util::runtime_error(SFA::util::error_code::RingbufferTooSlowOrNotBigEnough,__FILE__,__func__);
        }
}