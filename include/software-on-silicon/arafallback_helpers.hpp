template<typename Piece> class PieceWriter {
    public:
    PieceWriter(SOS::MemoryView::RingBufferBus<Piece>& bus) : myBus(bus) {}
    void write(SAMPLE_SIZE* buffer, const unsigned int position, const unsigned int length){
        auto current = std::get<0>(myBus.cables).getCurrentRef().load();
        const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
        const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            //write directly to HOSTmemory
            std::get<0>(*current)=length;
            std::get<2>(*current)=position;
            for (int i=0;i<length;i++){
                std::get<1>(*current)[i]=buffer[i];
            }
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            //write last bit
            std::get<0>(*current)=length;
            std::get<2>(*current)=position;
            for (int i=0;i<length;i++){
                std::get<1>(*current)[i]=buffer[i];
            }
            //do not advance -> current invalid  
            std::cout<<std::endl;
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
    }
    private:
    SOS::MemoryView::RingBufferBus<Piece>& myBus;
};
