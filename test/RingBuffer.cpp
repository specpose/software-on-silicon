#include "software-on-silicon/helpers.hpp"
#include <iostream>
#include <chrono>

#define RING_BUFFER std::array<char,33>

using namespace SOS::MemoryView;

struct RingBufferBusImpl : public RingBufferBus<RING_BUFFER> {
    using RingBufferBus<RING_BUFFER>::RingBufferBus;
};
class RingBufferTaskImpl : protected SOS::Behavior::RingBufferTask<RingBufferBusImpl> {
    public:
    using cable_type = std::tuple_element<0,RingBufferBusImpl::cables_type>::type;
    using const_cable_type = std::tuple_element<0,RingBufferBusImpl::const_cables_type>::type;
    RingBufferTaskImpl(cable_type& indices, const_cable_type& bounds) : SOS::Behavior::RingBufferTask<RingBufferBusImpl>(indices, bounds){}
    protected:
    virtual void write(const char character) override {std::cout<<character;}
};
class RingBufferImpl : private SOS::RingBufferLoop, public RingBufferTaskImpl {
    public:
    RingBufferImpl(RingBufferBusImpl& bus) :
    SOS::RingBufferLoop(bus.signal),
    RingBufferTaskImpl(std::get<0>(bus.cables),std::get<0>(bus.const_cables))
    {
        _thread = start(this);
    }
    ~RingBufferImpl() final{
        stop_requested=true;
        _thread.join();
    }
    void event_loop(){
        while(!stop_requested){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                RingBufferTask::read_loop();
            }
        }
    }
    private:
    bool stop_requested = false;
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};
template<typename Piece> class PieceWriter {
    public:
    PieceWriter(RingBufferBusImpl& bus) : myBus(bus) {}
    //offset: goes to MemoryController->BKPos if combined!
    void writePiece(typename Piece::difference_type offset, typename Piece::difference_type length){
        myBus.setLength(length);//Reader length!
        auto current = std::get<0>(myBus.cables).getCurrentRef().load();
        const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
        const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
        const auto writeLength = std::get<1>(myBus.cables).getWriteLengthRef().load();
        if (writeLength>=std::distance(current,end)+std::distance(start,current)){
            throw SFA::util::runtime_error("Individual write length too big or RingBuffer too small",__FILE__,__func__);
        }
        for (typename Piece::difference_type i= 0; i<writeLength;i++){//Lock-free (host) write length!
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            std::cout<<"=";
            //write directly to HOSTmemory
            *current='+';
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            //*current='+';
            std::cout<<std::endl;
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        }
    }
    private:
    RingBufferBusImpl& myBus;
};

using namespace std::chrono;

int main(){
    auto hostmemory = RING_BUFFER{};
    auto bus = RingBufferBusImpl(hostmemory.begin(),hostmemory.end());
    auto hostwriter = PieceWriter<decltype(hostmemory)>(bus);
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
    const auto beginning = high_resolution_clock::now();
    //try {
    hostwriter.writePiece(0,32);
    std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
    }
    std::cout<<std::endl;
    /*} catch (std::exception& e) {
        delete buffer;
        buffer = nullptr;
    }*/
    if (buffer!=nullptr)
        delete buffer;
}