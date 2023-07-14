/*
    This class is for reading from a driver or gpio on a ControllerHost into a RingBuffer and providing random
    memory access from either the ControllerHost, or a fpga SubController propped onto the Reader

    ControllerHost<Writer<Reader<SubController>>>

    It is not suitable for reading from a FPGA gpio when the processing needs immediate, timed pre-processing because of the signaling

    ControllerHost<Reader<SigmaDelta<Writer(GPIO)>>>
*/

#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"
#include <chrono>

#define RING_BUFFER std::array<char,334>
#define MEMORY_CONTROLLER std::array<char,10000>
#define READ_BUFFER std::array<char,1000>

using namespace SOS;

class ReaderImpl : public SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>& blockerbus):
    SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>(outside, blockerbus)
    {
        _thread = start(this);
    }
    ~ReaderImpl(){
        stop_requested = true;
        _thread.join();
    }
    private:
    std::thread _thread;
};
class WriteTaskImpl : public SOS::Behavior::WriteTask<MEMORY_CONTROLLER> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<MEMORY_CONTROLLER>() {
        this->memorycontroller.fill('-');
    }
};
class TransferRingToMemory : protected Behavior::RingBufferTask<RING_BUFFER>, protected WriteTaskImpl {
    public:
    TransferRingToMemory(
        Behavior::RingBufferTask<RING_BUFFER>::cable_type& indices,
        Behavior::RingBufferTask<RING_BUFFER>::const_cable_type& bounds
        ) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds), WriteTaskImpl{} {}
    protected:
    //multiple inheritance: ambiguous override!
    virtual void write(const MEMORY_CONTROLLER::value_type character) final {
        _blocker.signal.getNotifyRef().clear();
        WriteTaskImpl::write(character);
        _blocker.signal.getNotifyRef().test_and_set();
        }
};
//SimpleLoop has only one constructor argument
//SimpleLoop does not forward passThru
//TransferPriority needs the _intrinsic
class TransferPriority :
protected TransferRingToMemory,
protected SOS::Behavior::Loop {
    public:
    using subcontroller_type = ReaderImpl;
    using bus_type = typename SOS::Behavior::SimpleLoop<subcontroller_type>::bus_type;//notifier
    TransferPriority(
        MemoryView::RingBufferBus<RING_BUFFER>& rB,
        MemoryView::ReaderBus<READ_BUFFER>& rd
    ) :
    _intrinsic(rB.signal),
    TransferRingToMemory(std::get<0>(rB.cables),std::get<0>(rB.const_cables)),
    _child(subcontroller_type{rd,_blocker})
    {}
    virtual ~TransferPriority(){};
    void event_loop(){
        while(!stop_requested){
            if(!_intrinsic.getNotifyRef().test_and_set()){
                RingBufferTask::read_loop();
            }
        }
    }
    protected:
    bool stop_requested = false;
    private:
    bus_type::signal_type& _intrinsic;
    subcontroller_type _child;
};
class RingBufferImpl : public TransferPriority {
    public:
    RingBufferImpl(MemoryView::RingBufferBus<RING_BUFFER>& rB,MemoryView::ReaderBus<READ_BUFFER>& rd) :
    TransferPriority(rB,rd)
    {
        _thread = start(this);
    }
    ~RingBufferImpl() final{
        stop_requested=true;
        _thread.join();
    }
    private:
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

using namespace std::chrono;

int main (){
    auto hostmemory = RING_BUFFER{};
    auto ringbufferbus = MemoryView::RingBufferBus<RING_BUFFER>(hostmemory.begin(),hostmemory.end());
    auto hostwriter = PieceWriter<decltype(hostmemory)>(ringbufferbus);

    auto randomread = READ_BUFFER{};
    auto readerBus = MemoryView::ReaderBus<READ_BUFFER>(randomread.begin(),randomread.end());

    std::cout << "Reader reading 1000 times at tail of memory at rate 1/s..." << std::endl;
    std::cout << "Writer writing 9990 times from start at rate 1/ms..." << std::endl;
    RingBufferImpl* buffer = new RingBufferImpl(ringbufferbus,readerBus);
    readerBus.setOffset(9000);//FIFO has to be called before each getUpdatedRef().clear()
    readerBus.signal.getUpdatedRef().clear();
    unsigned int count = 0;
    auto loopstart = high_resolution_clock::now();
    try {
    //3000: read and write are meant to be called from independent controllers
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto beginning = high_resolution_clock::now();
        //read
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.setOffset(9000);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << *print++;
            std::cout << std::endl;
        }
        //write
        switch(count++){
            case 0:
                hostwriter.writePiece('*', 333);//lock free write
                break;
            case 1:
                hostwriter.writePiece('_', 333);
                break;
            case 2:
                hostwriter.writePiece('_', 333);
                count=0;
                break;
        }
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{333}));
    }
    } catch (std::exception& e) {
        std::cout<<std::endl<<"RingBuffer Shutdown"<<std::endl;
        delete buffer;
        buffer = nullptr;
    }
    if (buffer!=nullptr)
        delete buffer;
}