#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/error.hpp"
#include <iostream>
#include <chrono>

using namespace SOS::MemoryView;
using namespace std::chrono;

class ReadTask {
    public:
    using reader_length_ct = std::tuple_element<0,ReaderBus::const_cables_type>::type;
    using reader_offset_ct = std::tuple_element<0,ReaderBus::cables_type>::type;
    using blocker_ct = std::tuple_element<0,BlockerBus::cables_type>::type;
    ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,BlockerBus& blockerbus) : _length(Length),_offset(Offset), _blocked(blockerbus) {}
    protected:
    void read(){
        auto current = _length.getReadBufferStartRef();
        const auto end = _length.getReadBufferAfterLastRef();
        const auto readOffset = _offset.getReadOffsetRef().load();
        if (std::distance(_blocked.start,_blocked.end)<(std::distance(current,end)+readOffset))
            throw SFA::util::runtime_error("Read index out of bounds",__FILE__,__func__);
        std::get<0>(_blocked.cables).getBKReaderPosRef().store(
                _blocked.start
                +readOffset
                );
        while (current!=end){
            if (!_blocked.signal.getNotifyRef().test_and_set()) {
                _blocked.signal.getNotifyRef().clear();
            } else {
                auto rP = std::get<0>(_blocked.cables).getBKReaderPosRef().load();
                *current = *rP;
                std::get<0>(_blocked.cables).getBKReaderPosRef().store(++rP);
                ++current;
            }
        }
        std::cout << "Read finished." << std::endl;
    }
    private:
    reader_length_ct& _length;
    reader_offset_ct& _offset;
    BlockerBus& _blocked;
};
class ReaderImpl : public SOS::Behavior::Reader, private ReadTask {
    public:
    ReaderImpl(bus_type& outside, BlockerBus& blockerbus) :
    SOS::Behavior::Reader(outside.signal),
    ReadTask(std::get<0>(outside.const_cables),std::get<0>(outside.cables),blockerbus)
    {
        std::cout << "Reader reading 1000 times at tail of memory at rate 1/s..." << std::endl;
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
            if (!_intrinsic.getUpdatedRef().test_and_set()){
                std::cout << "Starting read." << std::endl;
                read();
                _intrinsic.getAcknowledgeRef().clear();
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
    using blocker_ct = std::tuple_element<0,BlockerBus::cables_type>::type;
    WriteTask(BlockerBus& blockerbus) : _item(blockerbus) {
        memorycontroller.fill('-');
        _item.start=memorycontroller.begin();
        _item.end=memorycontroller.end();
        std::get<0>(_item.cables).getBKPosRef().store(memorycontroller.begin());
        std::get<0>(_item.cables).getBKReaderPosRef().store(memorycontroller.begin());
    }
    protected:
    void write(const char character){
        auto pos = std::get<0>(_item.cables).getBKPosRef().load();
        if (pos!=memorycontroller.end()) {
            _item.signal.getNotifyRef().clear();
            *(pos++)=character;
            std::get<0>(_item.cables).getBKPosRef().store(pos);
            _item.signal.getNotifyRef().test_and_set();
        } else {
            /*auto print = memorycontroller.begin();
            while (print!=memorycontroller.end())
                std::cout << *print++;
            std::cout << std::endl;*/
            //throw causes memory access violation in Reader
            throw SFA::util::logic_error("Writer Buffer full",__FILE__,__func__);
        }
    }
    private:
    BlockerBus& _item;
    std::array<char,10000> memorycontroller = std::array<char,10000>{};
};
class WritePriorityImpl : private SOS::Behavior::WritePriority<ReaderImpl>, private WriteTask {
    public:
    WritePriorityImpl(
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::bus_type& writer,
        typename SOS::Behavior::SimpleLoop<ReaderImpl>::subcontroller_type::bus_type& passThruHostMem
        ) : SOS::Behavior::WritePriority<ReaderImpl>(writer.signal, passThruHostMem), WriteTask(_blocker) {
            
            std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
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
        if (!_intrinsic.getNotifyRef().test_and_set()){
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
    readerBus.setOffset(9000);
    auto controller = new WritePriorityImpl(writerBus,readerBus);
    readerBus.signal.getUpdatedRef().clear();
    while (true){
        writerBus.signal.getNotifyRef().clear();
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            auto print = hostmemory.begin();
            while (print!=hostmemory.end())
                std::cout << *print++;
            std::cout << std::endl;
            readerBus.signal.getUpdatedRef().clear();
        }
    }
    if (controller!=nullptr)
        delete controller;
}