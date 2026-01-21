#include "RingToMemory.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

using BLINK_T = std::array<std::tuple_element<0,RING_BUFFER::value_type>::type,1>;

//Helper classes
class Functor1 {
    public:
    Functor1(SOS::MemoryView::ReaderBus<BLOCK>& readerBus) : _readerBus(readerBus), buffer(RingBufferImpl{ringbufferbus,_readerBus}) {
        std::fill(std::begin(blink),std::end(blink),BLINK_T::value_type{{2}});
        std::fill(std::begin(noblink),std::end(noblink),BLINK_T::value_type{{1}});
        _thread = std::thread{std::mem_fn(&Functor1::test_loop),this};
    }
    ~Functor1(){
        _thread.join();
        //_readerBus.signal.getAcknowledgeRef().clear();//HACK: while -> thread.yield
    }
    void operator()(BLINK_T& channel_ptrs, const std::size_t actualSamplePosition){
        for (std::size_t i=0; i<std::tuple_size<BLINK_T>{}; i++)
            WriteInterleaved<decltype(hostmemory),BLINK_T::value_type>(ringbufferbus,channel_ptrs[i], actualSamplePosition);
    }
    void test_loop(){
        std::size_t actualSamplePosition = 0;
        const std::size_t numSamples = 333;//std::tuple_size<BLINK_T>{}
        auto loopstart = high_resolution_clock::now();
        //try {
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<9) {
            const auto beginning = high_resolution_clock::now();
            switch(count){
                case 0:
                    operator()(blink,actualSamplePosition);//lock free write
                    count++;
                    break;
                case 1:
                    operator()(noblink,actualSamplePosition);//lock free write
                    count++;
                    break;
                case 2:
                    operator()(noblink,actualSamplePosition);//lock free write
                    count=0;
                    break;
            }
            actualSamplePosition += std::tuple_size<BLINK_T>{};//vst actualSamplePosition
            std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{std::tuple_size<BLINK_T>{}}));
        }
        //} catch (std::exception& e) {
        //std::cout<<std::endl<<"RingBuffer Shutdown"<<std::endl;
        //}
    }
    MEMORY_CONTROLLER::difference_type ara_sampleCount() {
        return buffer.ara_sampleCount;
    }
    private:
    BLINK_T blink{};
    BLINK_T noblink{};

    SOS::MemoryView::ReaderBus<BLOCK>& _readerBus;

    RING_BUFFER hostmemory = RING_BUFFER{{{{0},0}}};//offset
    SOS::MemoryView::RingBufferBus<RING_BUFFER> ringbufferbus{hostmemory.begin(),hostmemory.end()};
    //if RingBufferImpl<ReaderImpl> shuts down too early, Piecewriter is catching up
    //=>Piecewriter needs readerimpl running
    RingBufferImpl buffer;

    unsigned int count = 0;
    //not strictly necessary, simulate real-world use-scenario
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
    const std::size_t ara_offset = 2996;

    SOS::MemoryView::ReaderBus<BLOCK> readerBus{};
    std::cout << "Reader reading "<<std::tuple_size<BLOCK>{}<<" characters per second at position " << ara_offset << "..." << std::endl;
    auto functor2 = Functor2(readerBus);
    BLOCK buffers{{{0},{0},{0},{0},{0}}};
    functor2.asyncRead(buffers, ara_offset);

    std::cout << "Writer writing 9990 times (10s) from start at rate 1/ms..." << std::endl;
    auto functor1 = Functor1(readerBus);

    auto loopstart = high_resolution_clock::now();
    auto beginning = loopstart;
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<6) {

        if (functor2() && (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - beginning).count() > 0)){
            beginning = high_resolution_clock::now();
            for (std::size_t i=0; i<std::tuple_size<BLOCK>{}; i++)
                std::cout << buffers[i].channels[0];//HACK: hard-coded channel 0
            std::cout << std::endl;
            functor2.asyncRead(buffers, ara_offset);
        }
    }
}
