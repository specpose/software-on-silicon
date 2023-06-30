#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>
#include <chrono>

using namespace SOS::MemoryView;
using namespace std::chrono;

class ReadTask {
    public:
    ReadTask(ReaderBus::cables_type& cables,SOS::MemoryView::BlockerBus& blockerbus) : _item(cables), _blocked(blockerbus) {

    }
    protected:
    void read(){
        auto current = get<ReadLength::wire_names::startPos>(std::get<0>(_item)).load();
        auto end = get<ReadLength::wire_names::afterLast>(std::get<0>(_item)).load();
        const auto readOffset = SOS::MemoryView::get<SOS::MemoryView::ReadOffset::wire_names::readOffset>(std::get<1>(_item)).load();
        if (std::distance(_blocked.start,_blocked.end)<(std::distance(current,end)+readOffset))
            throw SFA::util::runtime_error("Read index out of bounds",__FILE__,__func__);
        SOS::MemoryView::get<SOS::MemoryView::BlockerCable::wire_names::readerPos>(std::get<0>(_blocked.cables)).store(
                _blocked.start
                +readOffset
                );
        while (current!=get<ReadLength::wire_names::afterLast>(std::get<0>(_item)).load()) {//can adjust -> vector possible
            if (!get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_blocked.signal).test_and_set()) {
                get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_blocked.signal).clear();
            } else {
                //auto p = SOS::MemoryView::get<SOS::MemoryView::BlockerCable::wire_names::pos>(std::get<0>(_blocked.cables)).load();
                auto rP = SOS::MemoryView::get<SOS::MemoryView::BlockerCable::wire_names::readerPos>(std::get<0>(_blocked.cables)).load();
                //if (p!=rP){
                    *current = *rP;
                    SOS::MemoryView::get<SOS::MemoryView::BlockerCable::wire_names::readerPos>(std::get<0>(_blocked.cables)).store(++rP);
                    get<ReadLength::wire_names::startPos>(std::get<0>(_item)).store(++current);
                //}
            }
        }
        std::cout << "Reader finished." << std::endl;
    }
    private:
    ReaderBus::cables_type& _item;
    SOS::MemoryView::BlockerBus& _blocked;
};
class ReaderImpl : public SOS::Behavior::Reader, private ReadTask {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus& blockerbus) :
    SOS::Behavior::Reader(outside.signal),
    ReadTask(outside.cables,blockerbus)
    {
        std::cout << "Reader reading sections 1/s..." << std::endl;
        _thread = start(this);
    }
    ~ReaderImpl(){
        stop_requested = true;
        _thread.join();
    }
    void event_loop(){
        std::cout << "Starting ReaderImpl event_loop." << std::endl;
        while(!stop_requested){
            const auto start = high_resolution_clock::now();
            if (!get<SOS::MemoryView::BusShaker::signal_type::signal::updated>(_intrinsic).test_and_set()){
                std::cout << "Starting read." << std::endl;
                read();
                get<SOS::MemoryView::BusShaker::signal_type::signal::acknowledge>(_intrinsic).clear();
            }
            std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(seconds{1}));
        }
    }
    private:
    bool stop_requested = false;
    std::thread _thread;
};
class WriteTask {
    public:
    WriteTask(SOS::MemoryView::BlockerBus& blockerbus) : _item(blockerbus) {
        memorycontroller.fill('-');
        _item.start=memorycontroller.begin();
        _item.end=memorycontroller.end();
        SOS::MemoryView::get<SOS::MemoryView::BlockerCable::wire_names::pos>(std::get<0>(_item.cables)).store(memorycontroller.begin());
        SOS::MemoryView::get<SOS::MemoryView::BlockerCable::wire_names::readerPos>(std::get<0>(_item.cables)).store(memorycontroller.begin());
    }
    protected:
    void write(const char character){
        auto pos = SOS::MemoryView::get<SOS::MemoryView::BlockerCable::wire_names::pos>(std::get<0>(_item.cables)).load();
        if (pos!=memorycontroller.end()) {
            get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_item.signal).clear();
            *(pos++)=character;
            SOS::MemoryView::get<SOS::MemoryView::BlockerCable::wire_names::pos>(std::get<0>(_item.cables)).store(pos);
            get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_item.signal).test_and_set();
        } else {
            /*auto print = memorycontroller.begin();
            while (print!=memorycontroller.end())
                std::cout << *print++;
            std::cout << std::endl;*/
            //throw causes memory access violation in Reader
            throw SFA::util::runtime_error("Writer Buffer full",__FILE__,__func__);
        }
    }
    private:
    SOS::MemoryView::BlockerBus& _item;
    std::array<char,10000> memorycontroller = std::array<char,10000>{};
};
class WritePriorityImpl : private SOS::Behavior::WritePriority<ReaderImpl>, private WriteTask {
    public:
    WritePriorityImpl(
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::bus_type& writer,
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::subcontroller_type::bus_type& passThruHostMem
        ) : SOS::Behavior::WritePriority<ReaderImpl>(writer.signal, passThruHostMem), WriteTask(_blocker) {
            
            std::cout << "Writer writing 10000 times at rate 1/ms..." << std::endl;
            _thread = start(this);
        };
    virtual ~WritePriorityImpl(){
        stop_requested = true;
        _thread.join();
    };
    void event_loop(){
        int counter = 0;
        bool blink = true;
        std::cout << "Starting WritePriorityImpl event_loop." << std::endl;
        while(!stop_requested){
        const auto start = high_resolution_clock::now();
        if (!SOS::MemoryView::get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_intrinsic).test_and_set()){
            if (blink)
                write('*');
            else
                write('_');
            counter++;
            if (blink && counter==333){
                blink = false;
                counter=0;
            } else if (!blink && counter==666) {
                blink = true;
                counter=0;
            }
        }
        std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
        }
    };
    private:
    bool stop_requested = false;
    std::thread _thread;
};

int main(){
    auto writerBus = BusNotifier{};
    auto hostmemory = std::array<char,1000>{};
    auto readerBus = ReaderBus(hostmemory.begin(),hostmemory.end());
    auto controller = new WritePriorityImpl(writerBus,readerBus);
    readerBus.setOffset(9000);
    get<BusShaker::signal_type::signal::updated>(readerBus.signal).clear();
    while (true){
        get<BusNotifier::signal_type::signal::notify>(writerBus.signal).clear();
        if(!get<BusShaker::signal_type::signal::acknowledge>(readerBus.signal).test_and_set()){
            auto pos = hostmemory.begin();
            while (pos!=hostmemory.end())
                std::cout << *pos++;
            std::cout << std::endl;
            get<SOS::MemoryView::ReadLength::wire_names::startPos>(std::get<0>(readerBus.cables)).store(hostmemory.begin());
            get<BusShaker::signal_type::signal::updated>(readerBus.signal).clear();
        }
    }
    if (controller!=nullptr)
        delete controller;
}