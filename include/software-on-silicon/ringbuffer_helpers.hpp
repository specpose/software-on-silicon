namespace SOSFloat {
template<typename Piece> void PieceWriter_write(typename Piece::iterator current, const SAMPLE_SIZE* buffer[], const std::size_t channels, const std::size_t sampleIndex){
    for (int channel=0;channel<channels;channel++)
        (**current)[channel]=buffer[channel][sampleIndex];
}
template<typename Piece> void PieceWriter(SOS::MemoryView::RingBufferBus<Piece>& myBus, const SAMPLE_SIZE* buffer[], const std::size_t channels, const std::size_t length){//value type, amount, offset, (value_detail)
    auto current = std::get<0>(myBus.cables).getCurrentRef().load();
    const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
    const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
    //std::cout<<"=";
    std::cout<<length;
    for (std::size_t i=0;i<length;i++){
    if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
        //write directly to HOSTmemory
        PieceWriter_write<Piece>(current,buffer,channels,i);
        ++current;
        if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
            current = std::get<0>(myBus.const_cables).getWriterStartRef();
        std::get<0>(myBus.cables).getCurrentRef().store(current);
        myBus.signal.getNotifyRef().clear();
    } else {
        //write last bit
        PieceWriter_write<Piece>(current,buffer,channels,i);
        //do not advance -> current invalid  
        std::cout<<std::endl;
        SFA::util::runtime_error(SFA::util::error_code::RingbufferTooSlowOrNotBigEnough, __FILE__, __func__);
    }
    }
}
//has different for{for{}}
template<typename Piece> void PieceWriter(SOS::MemoryView::RingBufferBus<Piece>& myBus, const SAMPLE_SIZE* buffer[], const std::size_t channels, const std::size_t length, const std::size_t position){//value type, amount, offset, (value_detail)
    auto current = std::get<0>(myBus.cables).getCurrentRef().load();
    const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
    const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
    //std::cout<<"=";
    if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
        //write directly to HOSTmemory
        std::get<1>(*current)=length;
        std::get<2>(*current)=position;
        for (std::size_t sample=0;sample<length;sample++){
            for (int channel=0;channel<channels;channel++){
                (*(std::get<0>(*current)[sample]))[channel]=buffer[channel][sample];
            }
        }
        ++current;
        if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
            current = std::get<0>(myBus.const_cables).getWriterStartRef();
        std::get<0>(myBus.cables).getCurrentRef().store(current);
        myBus.signal.getNotifyRef().clear();
    } else {
        //write last bit
        std::get<1>(*current)=length;
        std::get<2>(*current)=position;
        for (std::size_t sample=0;sample<length;sample++){
            for (int channel=0;channel<channels;channel++){
                (*(std::get<0>(*current)[sample]))[channel]=buffer[channel][sample];
            }
        }
        //do not advance -> current invalid
        std::cout<<std::endl;
        throw SFA::util::runtime_error(SFA::util::error_code::RingbufferTooSlowOrNotBigEnough,__FILE__,__func__);
    }
}
}