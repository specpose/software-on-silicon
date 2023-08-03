namespace SOSFloat {
template<typename Piece> class PieceWriter {
    public:
    PieceWriter(SOS::MemoryView::RingBufferBus<Piece>& bus) : myBus(bus) {}
    void operator()(const SAMPLE_SIZE* buffer[], const unsigned int channels, const unsigned int position, const unsigned int length){//value type, amount, offset, (value_detail)
        auto current = std::get<0>(myBus.cables).getCurrentRef().load();
        const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
        const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            std::cout<<"=";
            //write directly to HOSTmemory
            std::get<0>(*current)=length;
            std::get<2>(*current)=position;
            for (int i=0;i<length;i++){
                auto entry = std::vector<SAMPLE_SIZE>(channels);
                for (int channel=0;channel<channels;channel++)
                    entry[channel]=buffer[channel][i];
                (*std::get<1>(*current))=entry;
            }
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            std::cout<<"=";
            //write last bit
            std::get<0>(*current)=length;
            std::get<2>(*current)=position;
            for (int i=0;i<length;i++){
                auto entry = std::vector<SAMPLE_SIZE>(channels);
                for (int channel=0;channel<channels;channel++)
                    entry[channel]=buffer[channel][i];
                (*std::get<1>(*current))=entry;
            }
            //do not advance -> current invalid  
            std::cout<<std::endl;
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
    }
    private:
    SOS::MemoryView::RingBufferBus<Piece>& myBus;
};
}