#include <iostream>

template<typename Piece> class PieceWriter {
    public:
    PieceWriter(SOS::MemoryView::RingBufferBus<Piece>& bus) : myBus(bus) {}
    void writePiece(typename Piece::value_type character, typename Piece::difference_type length){
        auto current = std::get<0>(myBus.cables).getCurrentRef().load();
        const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
        const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
        if (length>=std::distance(current,end)+std::distance(start,current)){
            throw SFA::util::runtime_error("Individual write length too big or RingBuffer too small",__FILE__,__func__);
        }
        for (typename Piece::difference_type i= 0; i<length;i++){//Lock-free (host) write length!
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            std::cout<<"=";
            //write directly to HOSTmemory
            *current=character;
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            //write last bit
            *current=character;
            //do not advance -> current invalid  
            std::cout<<std::endl;
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        }
    }
    private:
    SOS::MemoryView::RingBufferBus<Piece>& myBus;
};
