#include "software-on-silicon/RingBuffer.hpp"
#include <iostream>
#include <chrono>

using namespace SOS::MemoryView;

class RingBufferTask : private SOS::Behavior::MemoryControllerWrite {
    public:
    using cable_type = std::tuple_element<0,RingBufferBus::cables_type>::type;
    using const_cable_type = std::tuple_element<0,RingBufferBus::const_cables_type>::type;
    RingBufferTask(cable_type& indices, const_cable_type& bounds) :
    _item(indices),
    _bounds(bounds)
    {}
    void read_loop() {
        auto threadcurrent = _item.getThreadCurrentRef().load();
        auto current = _item.getCurrentRef().load();
        bool stop = false;
        while(!stop){//if: possible less writes than reads
            ++threadcurrent;
            if (threadcurrent==_bounds.getWriterEndRef())
                threadcurrent=_bounds.getWriterStartRef();
            if (threadcurrent!=current) {
                write(*threadcurrent);
                _item.getThreadCurrentRef().store(threadcurrent);
            } else {
                stop = true;
            }
        }
    }
    private:
    void write(const char character) override {std::cout<<character;}
    cable_type& _item;
    const_cable_type& _bounds;
};

class RingBufferImpl : private SOS::RingBufferLoop, public RingBufferTask {
    public:
    RingBufferImpl(RingBufferBus& bus) :
    SOS::RingBufferLoop(bus.signal),
    RingBufferTask(std::get<0>(bus.cables),std::get<0>(bus.const_cables))
    {
        _thread = start(this);
        std::cout<<"RingBuffer started"<<std::endl;
    }
    ~RingBufferImpl() final{
        stop_requested=true;
        _thread.join();
        std::cout<<"RingBuffer shutdown"<<std::endl;
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
    PieceWriter(RingBufferBus& bus) : myBus(bus) {}
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
    RingBufferBus& myBus;
};

using namespace std::chrono;

int main(){
    auto hostmemory = std::array<char,33>{};
    auto bus = RingBufferBus(hostmemory.begin(),hostmemory.end());
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