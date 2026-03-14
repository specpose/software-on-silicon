#define MAX_BLINK 333
#define BLOCK_SIZE 1000
#define SAMPLE_TYPE float
#define SAMPLE_RATE 1000
#define NUM_CHANNELS 1
#include "RingToMemory.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

#define TOTAL_TIME 7

//Helper classes
class Functor1 {
    public:
    Functor1(SOS::MemoryView::ReaderBus<BLOCK>& readerBus) : _readerBus(readerBus), buffer(ringbufferbus,_readerBus) {
        std::fill(std::begin(blink),std::end(blink),BLINK_T::value_type{{2}});
        std::fill(std::begin(noblink),std::end(noblink),BLINK_T::value_type{{1}});
        _thread = std::thread{std::mem_fn(&Functor1::test_loop),this};
    }
    ~Functor1(){
        _thread.join();
        //_readerBus.signal.getAcknowledgeRef().clear();//HACK: while -> thread.yield
    }
    void operator()(const BLINK_T& channel_ptrs){
        WriteInterleaved<decltype(hostmemory),BLINK_T>(ringbufferbus,channel_ptrs);
    }
    void test_loop(){
        auto loopstart = high_resolution_clock::now();
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<TOTAL_TIME) {
            const auto now = high_resolution_clock::now();
            if (std::chrono::duration<double, std::ratio<1>>(now - last).count() > double(std::tuple_size<BLINK_T>{})/double(SAMPLE_RATE)) {
            last = now;
            switch(count){
                case 0:
                    operator()(blink);//lock free write
                    count++;
                    break;
                case 1:
                    operator()(noblink);//lock free write
                    count++;
                    break;
                case 2:
                    operator()(noblink);//lock free write
                    count=0;
                    break;
            }
            }
            std::this_thread::yield();
        }
    }
    private:
    std::chrono::time_point<high_resolution_clock> last = high_resolution_clock::now();
    BLINK_T blink{};
    BLINK_T noblink{};

    SOS::MemoryView::ReaderBus<BLOCK>& _readerBus;

    RING_BUFFER hostmemory = RING_BUFFER{{{0}}};//offset
    SOS::MemoryView::RingBufferBus<RING_BUFFER> ringbufferbus{hostmemory.begin(),hostmemory.end()};
    RingBufferImpl buffer;

    unsigned int count = 0;
    std::thread _thread;
};
class Functor2 {
    public:
    Functor2(SOS::MemoryView::ReaderBus<BLOCK>& readerBus) : _readerBus(readerBus) {}
    ~Functor2(){
        //while(_readerBus.signal.getAcknowledgeRef().test_and_set())
        //    std::this_thread::yield();
        //_readerBus.signal.getUpdatedRef().test_and_set();
    };
    void asyncRead(BLOCK& buffer, const std::size_t offset){
        _readerBus.setReadBuffer(buffer);
        _readerBus.setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear()
        _readerBus.signal.getUpdatedRef().clear();
    }
    bool operator()() {
        if(!_readerBus.signal.getAcknowledgeRef().test_and_set()){
            return false;
        } else {
            return true;
        }
    }
    private:
    SOS::MemoryView::ReaderBus<BLOCK>& _readerBus;
};

using namespace std::chrono;

int main (){
    const std::size_t ara_offset = 3995*SAMPLE_RATE/1000;//2996

    SOS::MemoryView::ReaderBus<BLOCK> readerBus{};
    std::cout << "Reader reading "<<std::tuple_size<BLOCK>{}<<" characters per second at position " << ara_offset << "..." << std::endl;
    auto functor2 = new Functor2(readerBus);
    BLOCK buffers{};
    //initialize entire block to zero on all channels
    for (std::size_t i = 0; i < std::tuple_size<BLOCK>{}; i++)
        for (std::size_t j = 0; j < BLOCK::value_type::num_channels; j++)
            buffers[i].channels[j] = BLOCK::value_type::sample_type(0);
    functor2->asyncRead(buffers, ara_offset);

    std::cout << "Writer writing "<<TOTAL_TIME<<"s from start at rate "<<std::tuple_size<BLINK_T>{}<<"/"<<SAMPLE_RATE<<"..." << std::endl;
    Functor1 functor1{readerBus};

    auto loopstart = high_resolution_clock::now();
    auto beginning = loopstart;
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<TOTAL_TIME) {

        if ((*functor2)() && (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - beginning).count() > 0)){
            beginning = high_resolution_clock::now();
            for (std::size_t i=0; i<std::tuple_size<BLOCK>{}; i+=SAMPLE_RATE/1000)//HACK
                std::cout << buffers[i].channels[0];//HACK: hard-coded channel 0
            std::cout << std::endl;
            functor2->asyncRead(buffers, ara_offset);
        }
    }
    delete functor2;
}
