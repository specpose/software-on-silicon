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

class Reader : public SOS::Behavior::EventLoop<SOS::Behavior::SubController>,
                    private SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    using bus_type = typename SOS::MemoryView::ReaderBus<READ_BUFFER>;
    Reader(bus_type& outside, MemoryView::BlockerBus<MEMORY_CONTROLLER>& blockerbus) :
    SOS::Behavior::EventLoop<SOS::Behavior::SubController>(outside.signal),
    SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),blockerbus)
    {
        _thread = start(this);
    }
    ~Reader(){
        stop_requested = true;
        _thread.join();
    }
    void event_loop(){
        while(!stop_requested){
            if (!_intrinsic.getUpdatedRef().test_and_set()){//random access call, FIFO
                std::cout << "S";
                read();//FIFO whole buffer with intermittent waits when write
                std::cout << "F";
                _intrinsic.getAcknowledgeRef().clear();
            }
        }
    }
    private:
    bool stop_requested = false;
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
    virtual void write(const char character) override {
        _blocker.signal.getNotifyRef().clear();
        WriteTaskImpl::write(character);
        _blocker.signal.getNotifyRef().test_and_set();
        }
};
//SimpleLoop has only one constructor argument
//SimpleLoop does not forward passThru
class TransferPriority :
protected TransferRingToMemory,
protected SOS::Behavior::SimpleLoop<SOS::Behavior::SubController> {
    public:
    using subcontroller_type = Reader;
    using bus_type = typename SOS::Behavior::SimpleLoop<subcontroller_type>::bus_type;//notifier
    TransferPriority(
        MemoryView::RingBufferBus<RING_BUFFER>& rB,
        MemoryView::ReaderBus<READ_BUFFER>& rd
    ) :
    SOS::Behavior::SimpleLoop<SOS::Behavior::SubController>(rB.signal),
    TransferRingToMemory(std::get<0>(rB.cables),std::get<0>(rB.const_cables)),
    _child(Reader{rd,_blocker})
    {}
    virtual ~TransferPriority(){};
    void event_loop(){}
    private:
    Reader _child;
};
class RingBufferImpl : private TransferPriority {
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