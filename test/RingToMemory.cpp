/*
    This class is for reading from a driver or gpio on a ControllerHost into a RingBuffer and providing random
    memory access from either the ControllerHost, or a fpga SubController propped onto the Reader

    ControllerHost<Writer<Reader<SubController>>>

    It is not suitable for reading from a FPGA gpio when the processing needs immediate, timed pre-processing because of the signaling

    ControllerHost<Reader<SigmaDelta<Writer(GPIO)>>>
*/

#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/loop_helpers.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/arafallback_helpers.hpp"
#include <chrono>

#define RING_BUFFER std::vector<std::pair<unsigned int,std::vector<double>>>
#define MEMORY_CONTROLLER std::vector<double>
#define READ_BUFFER std::vector<double>

using namespace SOS;

class ReaderImpl : public SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>,
                    private SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    ReaderImpl(bus_type& blockerbus,SOS::MemoryView::ReaderBus<READ_BUFFER>& outside):
    SOS::Behavior::Reader<READ_BUFFER,MEMORY_CONTROLLER>(blockerbus, outside),
    SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER>(std::get<0>(outside.const_cables),std::get<0>(outside.cables),std::get<0>(blockerbus.cables))
    {
        _thread = start(this);
    }
    ~ReaderImpl(){
        stop_requested = true;
        _thread.join();
    }
    void event_loop() final {
        while(!stop_requested){
            fifo_loop();
            std::this_thread::yield();
        }
    }
    void fifo_loop() {
        if (!_intrinsic.getUpdatedRef().test_and_set()){//random access call, FIFO
//                        std::cout << "S";
            SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER>::read();//FIFO whole buffer with intermittent waits when write
//                        std::cout << "F";
            _intrinsic.getAcknowledgeRef().clear();
        }
    }
    bool wait() final {
        if (!_blocked_signal.getNotifyRef().test_and_set()) {//intermittent wait when write
            _blocked_signal.getNotifyRef().clear();
            return true;
        } else {
            return false;
        }
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
class RingBufferImpl : public TransferRingToMemory, protected SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>> {
    public:
    RingBufferImpl(MemoryView::RingBufferBus<RING_BUFFER>& rB,MemoryView::ReaderBus<READ_BUFFER>& rd) :
    TransferRingToMemory(std::get<0>(rB.cables),std::get<0>(rB.const_cables)),
    SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>(rB.signal,_blocker,rd)
    {
        //multiple inheritance: ambiguous base-class call
        _thread = SOS::Behavior::PassthruSimpleController<ReaderImpl, SOS::MemoryView::ReaderBus<READ_BUFFER>>::start(this);
    }
    ~RingBufferImpl() final{
        stop_requested=true;
        _thread.join();
    }
    //Overriding PassthruSimpleController
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
    //ALWAYS has to be private
    //ALWAYS has to be member of the upper-most superclass where _thread.join() is
    std::thread _thread = std::thread{};
};

//Helper classes
class Functor1 {
    public:
    Functor1(MemoryView::ReaderBus<READ_BUFFER>& readerBus, bool start=false) : _readerBus(readerBus){
        hostmemory.reserve(334);
        while(hostmemory.size()<334)
            hostmemory.push_back( std::pair(actualProcessSize,std::vector<double>(maxSamplesPerProcess)) );
        if (start)
            _thread = std::thread{std::mem_fn(&Functor1::operator()),this};
    }
    ~Functor1(){
        _thread.join();
    }
    void operator()(){
        auto loopstart = high_resolution_clock::now();
        //try {
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
            const auto beginning = high_resolution_clock::now();
            auto piece = std::vector<double>(maxSamplesPerProcess);
            switch(count++){
                case 0:
                    std::fill(piece.begin(),piece.begin()+actualProcessSize,1.0);
                    break;
                case 1:
                    std::fill(piece.begin(),piece.begin()+actualProcessSize,0.0);
                    break;
                case 2:
                    std::fill(piece.begin(),piece.begin()+actualProcessSize,0.0);
                    count=0;
                    break;
            }
            hostwriter.write(std::pair(actualProcessSize,piece));//lock free write
            std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{333}));
        }
        //} catch (std::exception& e) {
        //std::cout<<std::endl<<"RingBuffer Shutdown"<<std::endl;
        //}
    }
    private:
    MemoryView::ReaderBus<READ_BUFFER>& _readerBus;

    RING_BUFFER hostmemory = RING_BUFFER{};
    MemoryView::RingBufferBus<RING_BUFFER> ringbufferbus{hostmemory.begin(),hostmemory.end()};
    //if RingBufferImpl<ReaderImpl> shuts down too early, Piecewriter is catching up
    //=>Piecewriter needs readerimpl running
    RingBufferImpl buffer{ringbufferbus,_readerBus};

    PieceWriter<decltype(hostmemory)> hostwriter{ringbufferbus};//not a thread!
    unsigned int count = 0;
    unsigned int maxSamplesPerProcess = 334;
    unsigned int actualProcessSize = 333;
    //not strictly necessary, simulate real-world use-scenario
    std::thread _thread = std::thread{};
};
class Functor2 {
    public:
    Functor2(bool start=false, int readOffset=0) : _readOffset(readOffset) {
        if (start) {
            readerBus.setOffset(_readOffset);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
            _thread = std::thread{std::mem_fn(&Functor2::operator()),this};
        }
    }
    ~Functor2(){
        _thread.join();
    }
    void operator()() {
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<5) {
        const auto beginning = high_resolution_clock::now();
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.setOffset(_readOffset);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << *print++;
            std::cout << std::endl;
        }
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1000}));
        }
    }
    MemoryView::ReaderBus<READ_BUFFER> readerBus{randomread.begin(),randomread.end()};
    private:
    READ_BUFFER randomread = READ_BUFFER{};
    int _readOffset = 0;
    //not strictly necessary, simulate real-world use-scenario
    std::thread _thread = std::thread{};
};

using namespace std::chrono;

int main (){
    const int offset = 2996;
    std::cout << "Reader reading 1000 characters per second at position " << offset << "..." << std::endl;
    //read
    auto functor2 = Functor2(true, offset);
    std::cout << "Writer writing 9990 times (10s) from start at rate 1/ms..." << std::endl;
    //write
    auto functor1 = Functor1(functor2.readerBus, true);
}