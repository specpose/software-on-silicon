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
#include "software-on-silicon/arafallback_helpers.hpp"
#include <chrono>

#define RING_BUFFER std::vector<std::pair<unsigned int,std::vector<double>>>
#define MEMORY_CONTROLLER std::vector<double>
#define READ_BUFFER std::vector<double>

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
        resize(10000);
    }
    virtual void resize(typename MEMORY_CONTROLLER::difference_type newsize){
        memorycontroller.reserve(newsize);
        //for(int i=0;i<newsize;i++)
        while(memorycontroller.size()<newsize)
            memorycontroller.push_back(0.0);
        std::get<0>(_blocker.cables).getBKStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef().store(memorycontroller.end());
    };
};
class TransferRingToMemory : protected Behavior::RingBufferTask<RING_BUFFER>, protected WriteTaskImpl {
    public:
    TransferRingToMemory(
        Behavior::RingBufferTask<RING_BUFFER>::cable_type& indices,
        Behavior::RingBufferTask<RING_BUFFER>::const_cable_type& bounds
        ) : SOS::Behavior::RingBufferTask<RING_BUFFER>(indices, bounds), WriteTaskImpl{} {}
    protected:
    //multiple inheritance: ambiguous override!
    virtual void write(const RING_BUFFER::value_type character) final {
        _blocker.signal.getNotifyRef().clear();
        for(int i=0;i<character.first;i++)
            WriteTaskImpl::write(character.second[i]);
        _blocker.signal.getNotifyRef().test_and_set();
        }
};
//SimpleLoop has only one constructor argument
//SimpleLoop does not forward passThru
class TransferPriority :
protected TransferRingToMemory,
protected SOS::Behavior::SimpleLoop<SOS::Behavior::SubController> {
    public:
    using subcontroller_type = ReaderImpl;
    using bus_type = typename SOS::Behavior::SimpleLoop<subcontroller_type>::bus_type;//notifier
    TransferPriority(
        MemoryView::RingBufferBus<RING_BUFFER>& rB,
        MemoryView::ReaderBus<READ_BUFFER>& rd
    ) :
    SOS::Behavior::SimpleLoop<SOS::Behavior::SubController>(rB.signal),
    TransferRingToMemory(std::get<0>(rB.cables),std::get<0>(rB.const_cables)),
    _child(subcontroller_type{rd,_blocker})
    {}
    virtual ~TransferPriority(){};
    void event_loop(){}
    private:
    subcontroller_type _child;
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
    unsigned int maxSamplesPerProcess = 334;
    unsigned int actualProcessSize = 333;
    int actualSamplePosition = 0;

    auto hostmemory = RING_BUFFER{};
    hostmemory.reserve(334);
    while(hostmemory.size()<334)
        hostmemory.push_back( std::pair(actualProcessSize,std::vector<double>(maxSamplesPerProcess)) );
    auto ringbufferbus = MemoryView::RingBufferBus<RING_BUFFER>(hostmemory.begin(),hostmemory.end());
    auto hostwriter = PieceWriter<decltype(hostmemory)>(ringbufferbus);

    auto randomread = READ_BUFFER{};
    randomread.reserve(1000);
    while(randomread.size()<1000)
        randomread.push_back(0.0);
    auto readerBus = MemoryView::ReaderBus<READ_BUFFER>(randomread.begin(),randomread.end());

    std::cout << "Reader reading 1000 times at tail of memory at rate 1/s..." << std::endl;
    std::cout << "Writer writing 9990 times from start at rate 1/ms..." << std::endl;
    RingBufferImpl* buffer = new RingBufferImpl(ringbufferbus,readerBus);
    readerBus.setOffset(9000);//FIFO has to be called before each getUpdatedRef().clear()
    readerBus.signal.getUpdatedRef().clear();
    unsigned int count = 0;
    auto loopstart = high_resolution_clock::now();
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
        auto piece = std::vector<double>(maxSamplesPerProcess);
        switch (count){
            case 0:
            std::fill(piece.begin(),piece.begin()+actualProcessSize,1.0);
            count++;
            break;
            case 1:
            std::fill(piece.begin(),piece.begin()+actualProcessSize,0.0);
            count++;
            break;
            case 2:
            std::fill(piece.begin(),piece.begin()+actualProcessSize,0.0);
            count=0;
            break;
        }
        hostwriter.write(std::pair(actualProcessSize,piece));//lock free write
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{333}));
    }
    if (buffer!=nullptr)
        delete buffer;
}