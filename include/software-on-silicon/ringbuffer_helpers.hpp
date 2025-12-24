template<typename Piece> void PieceWriter_write(typename Piece::iterator current, const typename Piece::value_type character){
    *current=character;
}
template<typename Piece> void PieceWriter(SOS::MemoryView::RingBufferBus<Piece>& myBus, const typename Piece::value_type character, typename Piece::difference_type length){
    auto current = std::get<0>(myBus.cables).getCurrentRef().load();
    const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
    const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
    if (length>=std::distance(current,end)+std::distance(start,current)){
        SFA::util::runtime_error(SFA::util::error_code::IndividualWritelengthTooBigOrRingBufferTooSmall,__FILE__,__func__);
    }
    for (typename Piece::difference_type i= 0; i<length;i++){//Lock-free (host) write length!
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            std::cout<<"=";
            //write directly to HOSTmemory
            PieceWriter_write<Piece>(current,character);
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            //write last bit
            *current=character;
            //current invalid => do not advance
            std::cout<<std::endl;
            SFA::util::runtime_error(SFA::util::error_code::RingbufferTooSlowOrNotBigEnough,__FILE__,__func__);
        }
    }
}