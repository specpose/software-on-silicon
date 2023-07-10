#include "software-on-silicon/RingBuffer.hpp"
#include <chrono>

#define RING_BUFFER std::array<char,33>
#define MEMORY_CONTROLLER std::array<char,10000>

using namespace SOS;

struct RingBufferBusImpl : public MemoryView::RingBufferBus<RING_BUFFER> {
    using MemoryView::RingBufferBus<RING_BUFFER>::RingBufferBus;
};
struct BlockerBusImpl : public MemoryView::BlockerBus<MEMORY_CONTROLLER> {
    using MemoryView::BlockerBus<MEMORY_CONTROLLER>::BlockerBus;
};
class WriteTaskImpl : public SOS::Behavior::WriteTask<MEMORY_CONTROLLER,BlockerBusImpl> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<MEMORY_CONTROLLER,BlockerBusImpl>() {
        this->memorycontroller.fill('-');
    }
};
class TransferRingToMemory : protected Behavior::RingBufferTask<RingBufferBusImpl>, protected WriteTaskImpl {
    public:
    TransferRingToMemory(
        Behavior::RingBufferTask<RingBufferBusImpl>::cable_type& indices,
        Behavior::RingBufferTask<RingBufferBusImpl>::const_cable_type& bounds
        ) : SOS::Behavior::RingBufferTask<RingBufferBusImpl>(indices, bounds), WriteTaskImpl{} {}
    protected:
    virtual void write(const char character) override {WriteTaskImpl::write(character);}
};
class RingBufferImpl : private SOS::RingBufferLoop, public TransferRingToMemory {
    public:
    RingBufferImpl(RingBufferBusImpl& bus) :
    SOS::RingBufferLoop(bus.signal),
    TransferRingToMemory(std::get<0>(bus.cables),std::get<0>(bus.const_cables))
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
        myBus.setSamplePosition(offset);
        myBus.setLength(length);//Reader length!
    }
    private:
    RingBufferBusImpl& myBus;
};

using namespace std::chrono;

int main (){
    auto hostmemory = std::array<char,334>{};
    auto bus = RingBufferBusImpl(hostmemory.begin(),hostmemory.end());
    auto hostwriter = PieceWriter<decltype(hostmemory)>(bus);
    RingBufferImpl* buffer = new RingBufferImpl(bus);
    auto loopstart = high_resolution_clock::now();
    auto offset = 0;
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto beginning = high_resolution_clock::now();
        hostwriter.writePiece(offset,333);
        offset+=999;
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{999}));
    }
    if (buffer!=nullptr)
        delete buffer;
}