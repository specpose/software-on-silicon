#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/RingBuffer.hpp"
#include "stackable-functor-allocation/error.h"
#include <iostream>
#include <chrono>

using namespace SOS::MemoryView;
using namespace std::chrono;

class ReaderImpl : public SOS::Behavior::Reader {
    public:
    ReaderImpl(bus_type& outside, SOS::MemoryView::BlockerBus& blockerbus) : SOS::Behavior::Reader(outside, blockerbus) {
        _thread = start(this);
    }
    ~ReaderImpl(){
        stop_requested = true;
        _thread.join();
    }
    void read(){
        std::cout << "Reader doing 1000 reads at 1/s..." << std::endl;
    }
    void event_loop(){
        while (!stop_requested){
        if (!get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_blocked.signal).test_and_set()) {
            get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_blocked.signal).clear();
        } else {
            //offset and memorycontroller size is set on initialization
            //read(atSamplePositionFromOffset)
            const auto start = high_resolution_clock::now();
            if (!get<SOS::MemoryView::BusShaker::signal_type::signal::updated>(_intrinsic).test_and_set()){
                read();
                std::this_thread::sleep_until(start + duration_cast<high_resolution_clock::duration>(seconds{1}));
                get<SOS::MemoryView::BusShaker::signal_type::signal::acknowledge>(_intrinsic).clear();
            }
        }
        }
    }
    private:
    bool stop_requested = false;
    std::thread _thread;
};
class WriteTask {
    protected:
    void write(const char character){
        if (pos!=memorycontroller.end()) {
            *(pos++)=character;
        } else {
            auto print = memorycontroller.begin();
            while (print!=memorycontroller.end())
                std::cout << *print++;
            std::cout << std::endl;
            throw SFA::util::runtime_error("OutputBuffer full",__FILE__,__func__);
        }
    }
    private:
    std::array<char,10000> memorycontroller = std::array<char,10000>{'-'};
    std::array<char,10000>::iterator pos = memorycontroller.begin();
};
class WritePriorityImpl : private SOS::Behavior::WritePriority<ReaderImpl>, private WriteTask {
    public:
    WritePriorityImpl(
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::bus_type& myBus,
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::subcontroller_type::bus_type& passThru
        ) : SOS::Behavior::WritePriority<ReaderImpl>(myBus, passThru) {
            std::cout << "Writer writing 10000 times at rate 1/ms..." << std::endl;
            SOS::MemoryView::get<SOS::MemoryView::BlockerCable::wire_names::readerPos>(std::get<0>(_blocker.cables)).store(0);
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
        if (!SOS::MemoryView::get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_intrinsic).test_and_set()){
            get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_blocker.signal).clear();
            if (blink)
                write('*');
            else
                write('_');
            get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_blocker.signal).test_and_set();
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
    auto readerBus = ReaderBus{};
    auto controller = new WritePriorityImpl(writerBus,readerBus);
    while (true){
        get<BusShaker::signal_type::signal::updated>(readerBus.signal).clear();
        get<BusNotifier::signal_type::signal::notify>(writerBus.signal).clear();
    }
    if (controller!=nullptr)
        delete controller;
}