#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/MemoryController.hpp"
#include <iostream>
#include <chrono>

#define MEMORY_CONTROLLER std::array<char,10000>
#define READ_BUFFER std::array<char,1000>

using namespace SOS::MemoryView;

class Reader : public SOS::Behavior::EventLoop<SOS::Behavior::SubController>,
                    private SOS::Behavior::ReadTask<READ_BUFFER,MEMORY_CONTROLLER> {
    public:
    using bus_type = typename SOS::MemoryView::ReaderBus<READ_BUFFER>;
    Reader(bus_type& outside, BlockerBus<MEMORY_CONTROLLER>& blockerbus) :
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
//RunLoop does not have a constructor argument
//RunLoop does not forward passThru
class WritePriority : protected WriteTaskImpl, public SOS::Behavior::Loop {
    public:
    using subcontroller_type = Reader;
    using bus_type = typename SOS::Behavior::RunLoop<subcontroller_type>::bus_type;//empty
    WritePriority(
        typename subcontroller_type::bus_type& passThru
        ) : WriteTaskImpl{}, _child(Reader{passThru,_blocker}) {};
    virtual ~WritePriority(){};
    void event_loop(){}
    private:
    Reader _child;
};
using namespace std::chrono;
class WritePriorityImpl : private WritePriority {
    public:
    WritePriorityImpl(
        typename SOS::Behavior::SimpleLoop<Reader>::subcontroller_type::bus_type& passThruHostMem
        ) :
        WritePriority{passThruHostMem} {
            _thread = start(this);
        };
    virtual ~WritePriorityImpl(){
        stop_requested = true;
        _thread.join();
    };
    void event_loop(){
        int counter = 0;
        bool blink = true;
        while(!stop_requested){
        const auto start = high_resolution_clock::now();
        char data;
        if (blink)
            data = '*';
        else
            data = '_';
        _blocker.signal.getNotifyRef().clear();
        write(data);
        _blocker.signal.getNotifyRef().test_and_set();
        counter++;
        if (blink && counter==333){
            blink = false;
            counter=0;
        } else if (!blink && counter==666) {
            blink = true;
            counter=0;
        }
        std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
        }
    };
    private:
    bool stop_requested = false;
    std::thread _thread;
};

int main(){
    auto randomread = READ_BUFFER{};
    auto readerBus = ReaderBus<READ_BUFFER>(randomread.begin(),randomread.end());
    std::cout << "Reader reading 1000 times at tail of memory at rate 1/s..." << std::endl;
    std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
    auto controller = new WritePriorityImpl(readerBus);
    readerBus.setOffset(9000);//FIFO has to be called before each getUpdatedRef().clear()
    readerBus.signal.getUpdatedRef().clear();
    while (true){
        const auto start = high_resolution_clock::now();
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.setOffset(9000);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << *print++;
            std::cout << std::endl;
        }
        std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(seconds{1}));
    }
    if (controller!=nullptr)
        delete controller;
}