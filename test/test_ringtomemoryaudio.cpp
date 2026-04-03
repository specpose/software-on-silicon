#define INTEL 1
#if INTEL
#define MAX_BLINK 9600
#define MAX_READ 32
#else
#define MAX_BLINK 9600
#define MAX_READ 8
#endif
#if INTEL
#define BLOCK_SIZE 48000
#else
#define BLOCK_SIZE 8000
#endif
#define SAMPLE_TYPE short
#if INTEL
#define SAMPLE_RATE 48000
#else
#define SAMPLE_RATE 8000
#endif
#define NUM_CHANNELS 2
#include "RingToMemory.cpp"
#include "software-on-silicon/alsa_helpers.hpp"
#include "software-on-silicon/ringbuffer_helpers_alsa.hpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

#define TOTAL_TIME 160

//Helper classes
class Functor1 {
    public:
    Functor1(SOS::MemoryView::ReaderBus<BLOCK>& readerBus) : _readerBus(readerBus), buffer(ringbufferbus,_readerBus) {
        _thread = std::thread{std::mem_fn(&Functor1::test_loop),this};
    }
    ~Functor1(){
        _thread.join();
        //_readerBus.signal.getAcknowledgeRef().clear();//HACK: while -> thread.yield
    }
    void operator()(snd_pcm_t *handle, std::size_t& frames_read, pollfd* ufds, unsigned int fd_count, snd_pcm_uframes_t& max){
        WriteInterleaved(ringbufferbus,handle,frames_read,ufds,fd_count,max);
    }
    void test_loop(){
        //std::size_t ringbuffer_index = 0;
        snd_pcm_uframes_t period_size = MAX_READ;//NUM_CHANNELS * 27;//notification interval

        std::size_t frames_read = 0;
        snd_pcm_uframes_t max = 0;

        auto driver = SOS::Audio::Linux::init(rate, &period_size, SND_PCM_ACCESS_MMAP_INTERLEAVED, true);
        auto poll = SOS::Audio::Linux::init_poll(std::get<0>(driver));
        SOS::Audio::Linux::start_pcm(std::get<0>(driver));
        auto start = std::chrono::high_resolution_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()-start).count()<10){
            operator()(std::get<0>(driver), frames_read, std::get<0>(poll), std::get<1>(poll), max);
            /*operator()(hostmemory[ringbuffer_index], std::get<0>(driver), frames_read, std::get<0>(poll), std::get<1>(poll), max);
            ringbuffer_index = ringbuffer_index == std::tuple_size<RING_BUFFER>{}-1 ? 0 : ++ringbuffer_index;*/
            std::this_thread::yield();
        }
        SOS::Audio::Linux::destroy(driver);
        SOS::Audio::Linux::destroy_poll(poll);
    }
    private:
    SOS::MemoryView::ReaderBus<BLOCK>& _readerBus;

    RING_BUFFER hostmemory = RING_BUFFER{{{0}}};//offset
    SOS::MemoryView::RingBufferBus<RING_BUFFER> ringbufferbus{hostmemory.begin(),hostmemory.end()};
    RingBufferImpl buffer;

    std::thread _thread;
};
class Functor2 {
    public:
    Functor2(SOS::MemoryView::ReaderBus<BLOCK>& readerBus) : _readerBus(readerBus) {}
    ~Functor2(){
        _readerBus.signal.getUpdatedRef().test_and_set();
    };
    void asyncRead(BLOCK& buffer, const std::size_t offset){
        _readerBus.setReadBuffer(buffer);
        _readerBus.setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear()
        _readerBus.signal.getUpdatedRef().clear();
    }
    bool operator()() {
        if (!_readerBus.signal.getAcknowledgeRef().test_and_set())
            return true;
        else
            return false;
    }
    private:
    SOS::MemoryView::ReaderBus<BLOCK>& _readerBus;
};

using namespace std::chrono;

int main (){
    const std::size_t ara_offset = 3995*SAMPLE_RATE/960;

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

    const auto loopstart = high_resolution_clock::now();
    auto beginning = loopstart;
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<TOTAL_TIME) {
        if (duration_cast<seconds>(high_resolution_clock::now()-beginning).count()>0) {
            if ((*functor2)()) {
                beginning = high_resolution_clock::now();
                for (std::size_t i=0; i<std::tuple_size<BLOCK>{}; i+=SAMPLE_RATE/960)
                    std::cout << buffers[i].channels[0];//HACK: hard-coded channel 0
                std::cout << std::endl;
                functor2->asyncRead(buffers, ara_offset);
            }
        }
        std::this_thread::yield();
    }
    delete functor2;
}
