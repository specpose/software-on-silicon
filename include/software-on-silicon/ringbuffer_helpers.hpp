namespace SOSFloat {
template<typename Piece> class PieceWriter {
    public:
    PieceWriter(SOS::MemoryView::RingBufferBus<Piece>& bus) : myBus(bus) {}
    void operator()(const SAMPLE_SIZE* buffer[], const std::size_t channels, const std::size_t length, const std::size_t position=0){//value type, amount, offset, (value_detail)
        auto current = std::get<0>(myBus.cables).getCurrentRef().load();
        const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
        const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
        //std::cout<<"=";
        std::cout<<length;
        for (std::size_t i=0;i<length;i++){//up to length 9
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            //write directly to HOSTmemory
            write(current,buffer,channels,i);
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            //write last bit
            write(current,buffer,channels,i);
            //do not advance -> current invalid  
            std::cout<<std::endl;
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        }
    }
    private:
    static void write(typename Piece::iterator current,const SAMPLE_SIZE* buffer[], const std::size_t channels, const std::size_t sampleIndex){
        for (int channel=0;channel<channels;channel++)
            (**current)[channel]=buffer[channel][sampleIndex];
    }
    //template<> static void write<RING_BUFFER::value_type>(RING_BUFFER::value_type current,const SAMPLE_SIZE* buffer[], const std::size_t channels, const std::size_t sampleIndex){

    //}
    SOS::MemoryView::RingBufferBus<Piece>& myBus;
};
}