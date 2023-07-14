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

#define RING_BUFFER std::vector<std::tuple<unsigned int,std::vector<double>, unsigned int>>
#define MEMORY_CONTROLLER std::vector<double>
#define READ_BUFFER std::vector<double>

using namespace SOS;


template<> class SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    using reader_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<READ_BUFFER>::const_cables_type>::type;
    using reader_offset_ct = typename std::tuple_element<0,typename SOS::MemoryView::ReaderBus<READ_BUFFER>::cables_type>::type;
    using memorycontroller_length_ct = typename std::tuple_element<0,typename SOS::MemoryView::BlockerBus<MEMORY_CONTROLLER>::cables_type>::type;
    //not variadic, needs _blocked.signal.getNotifyRef()
    ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,memorycontroller_length_ct& blockercable) : _size(Length),_offset(Offset), _memorycontroller_size(blockercable) {}
    protected:
    void read(){
        //readbuffer
        const auto start = _size.getReadBufferStartRef();
        auto current = start;
        const auto end = _size.getReadBufferAfterLastRef();
        //memorycontroller
        const auto readOffset = _offset.getReadOffsetRef().load();
        const auto readerStart = _memorycontroller_size.getBKStartRef().load();
        const auto readerEnd = _memorycontroller_size.getBKEndRef().load();
        auto readerPos = readerStart+readOffset;
        std::cout<<"MemoryController size is "<<std::distance(readerStart,readerEnd)<<", distance of offset to end is "<<std::distance(readerPos,readerEnd)<<std::endl;
        while (current!=end){
            if (!wait()) {
                //if the distance of the lval from its start is bigger than
                //the (the rval offset to rval end)
                if (std::distance(start,current)>=std::distance(readerStart+readOffset,readerEnd))
                    *current = 0.0;
                else
                    *current = *(readerPos++);
                ++current;
            }
        }
    }
    virtual bool wait()=0;
    private:
    reader_length_ct& _size;
    reader_offset_ct& _offset;
    memorycontroller_length_ct& _memorycontroller_size;
};
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
class WriteTaskImpl : public SOS::Behavior::WriteTask<std::tuple_element<1,RING_BUFFER::value_type>::type> {
    public:
    WriteTaskImpl() : SOS::Behavior::WriteTask<std::tuple_element<1,RING_BUFFER::value_type>::type>() {
        //resize(10000);
        resize(0);
    }
    virtual void resize(typename std::tuple_element<1,RING_BUFFER::value_type>::type::difference_type newsize){
        memorycontroller.reserve(newsize);
        //for(int i=0;i<newsize;i++)
        while(memorycontroller.size()<newsize)
            memorycontroller.push_back(0.0);
        std::get<0>(_blocker.cables).getBKStartRef().store(memorycontroller.begin());
        std::get<0>(_blocker.cables).getBKEndRef().store(memorycontroller.end());
    };
    //helper function, not inherited
    void write(const RING_BUFFER::value_type character) {
        resize(std::get<0>(character)+std::get<2>(character));
        if (std::distance(std::get<0>(_blocker.cables).getBKStartRef().load(),std::get<0>(_blocker.cables).getBKEndRef().load())<
        std::get<2>(character)+std::get<0>(character))
            throw SFA::util::runtime_error("Writer tried to write beyond memorycontroller bounds",__FILE__,__func__);
        writerPos = std::get<0>(_blocker.cables).getBKStartRef().load() + std::get<2>(character);
        for(int i=0;i<std::get<0>(character);i++)
            SOS::Behavior::WriteTask<std::tuple_element<1,RING_BUFFER::value_type>::type>::write(std::get<1>(character)[i]);
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
    virtual void write(const RING_BUFFER::value_type character) final {
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

void fill(double* buffer, const unsigned int length, const double value){
    for (int i=0;i<length;i++)
        buffer[i]=value;
};

using namespace std::chrono;

int main (){
    unsigned int maxSamplesPerProcess = 334;
    auto AudioBuffer = new double[maxSamplesPerProcess];
    unsigned int actualProcessSize = 333;
    int actualSamplePosition = 0;
    int randomReadOffset=9990-667;//8500;

    auto hostmemory = RING_BUFFER{};
    hostmemory.reserve(2);
    std::cout<<"Hostmemory size "<<hostmemory.size()<<std::endl;
    while(hostmemory.size()<2)
        hostmemory.push_back( std::tuple(0,std::vector<double>(maxSamplesPerProcess),0) );
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
    readerBus.setOffset(randomReadOffset);//FIFO has to be called before each getUpdatedRef().clear()
    readerBus.signal.getUpdatedRef().clear();
    unsigned int count = 0;
    auto loopstart = high_resolution_clock::now();
    //3000: read and write are meant to be called from independent controllers
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto beginning = high_resolution_clock::now();
        //read
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.setOffset(randomReadOffset);//FIFO has to be called before each getUpdatedRef().clear()
            std::cout<<"Reading offset "<<randomReadOffset<<":"<<std::endl;
            readerBus.signal.getUpdatedRef().clear();
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << *print++;
            std::cout << std::endl;
        }
        //write
        //auto piece = std::vector<double>(maxSamplesPerProcess);
        switch (count){
            case 0:
            fill(AudioBuffer,actualProcessSize,1.0);
            //std::fill(piece.begin(),piece.begin()+actualProcessSize,1.0);
            count++;
            break;
            case 1:
            fill(AudioBuffer,actualProcessSize,0.0);
            //std::fill(piece.begin(),piece.begin()+actualProcessSize,0.0);
            count++;
            break;
            case 2:
            fill(AudioBuffer,actualProcessSize,0.0);
            //std::fill(piece.begin(),piece.begin()+actualProcessSize,0.0);
            count=0;
            break;
        }
        hostwriter.write(AudioBuffer,actualSamplePosition,actualProcessSize);//lock free write
        std::cout<<"Written samplePosition "<<actualSamplePosition<<" to ringbuffer."<<std::endl;
        actualSamplePosition += actualProcessSize;
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{333}));
    }
    if (buffer!=nullptr)
        delete buffer;
}