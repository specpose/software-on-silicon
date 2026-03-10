#define MAX_BLINK 1
#define STORAGE_SIZE 10000
#define BLOCK_SIZE 1000
#define SAMPLE_TYPE char
#define NUM_CHANNELS 1
#include "RingToMemory.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

#define TOTAL_TIME 10

//Helper classes
class Functor1 {
    public:
    Functor1(SOS::MemoryView::ReaderBus<BLOCK>& readerBus) : _readerBus(readerBus), buffer(ringbufferbus,_readerBus) {
        _thread = std::thread{std::mem_fn(&Functor1::operator()),this};
    }
    ~Functor1(){
        _thread.join();
        //_readerBus.signal.getAcknowledgeRef().clear();//HACK: while -> thread.yield
    }
    void operator()(){
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<TOTAL_TIME) {
            const auto now = high_resolution_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() > std::tuple_size<BLINK_T>{} ) {
            last = now;
            RING_BUFFER::value_type blink{};
            switch(count){
                case 0:
                    blink={'*'};
                    count++;
                    break;
                case 1:
                    blink={'_'};
                    count=0;
                    break;
            }
            WriteInterleaved<decltype(hostmemory)>(ringbufferbus,blink);
            std::this_thread::yield();
            }
        }
    }
    private:
    std::chrono::time_point<high_resolution_clock> last = high_resolution_clock::now();

    SOS::MemoryView::ReaderBus<BLOCK>& _readerBus;

    RING_BUFFER hostmemory = RING_BUFFER{{0}};
    SOS::MemoryView::RingBufferBus<RING_BUFFER> ringbufferbus{hostmemory.begin(),hostmemory.end()};
    RingBufferImpl buffer;

    unsigned int count = 0;
    std::thread _thread;
};
class Functor2 {
    public:
    Functor2(const std::size_t readOffset=0) : _readOffset(readOffset) {
        readerBus.setOffset(_readOffset);
        readerBus.signal.getUpdatedRef().clear();
        _thread = std::thread{std::mem_fn(&Functor2::operator()),this};
    }
    ~Functor2(){
        _thread.join();
    }
    void operator()() {
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<TOTAL_TIME) {
        const auto beginning = high_resolution_clock::now();
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.signal.getUpdatedRef().clear();//offset change omitted
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << (print++)->channels[0];
            std::cout << std::endl;
        }
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1000}));
        }
    }
    private:
    BLOCK randomread{};
    public:
    SOS::MemoryView::ReaderBus<decltype(randomread)> readerBus{randomread.begin(),randomread.end()};
    private:
    const std::size_t _readOffset = 0;
    //not strictly necessary, simulate real-world use-scenario
    std::thread _thread;
};

using namespace std::chrono;

int main (){
    const std::size_t ara_offset = 2996;
    std::cout << "Reader reading "<<std::tuple_size<BLOCK>{}<<" characters per second at position " << ara_offset << "..." << std::endl;
    Functor2 functor2(ara_offset);
    std::cout << "Writer writing 4995 times (5s) from start at rate 1/ms..." << std::endl;
    Functor1 functor1(functor2.readerBus);
}
