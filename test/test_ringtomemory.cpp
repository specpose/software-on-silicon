#include "RingToMemory.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

//Helper classes
class Functor1 {
    public:
    Functor1(MemoryView::ReaderBus<READ_BUFFER>& readerBus, bool start=false) : _readerBus(readerBus){
        if (start)
            _thread = std::thread{std::mem_fn(&Functor1::operator()),this};
    }
    ~Functor1(){
        _thread.join();
        _readerBus.signal.getAcknowledgeRef().clear();//HACK: while -> thread.yield
    }
    void operator()(){
        auto loopstart = high_resolution_clock::now();
        //try {
        while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<9) {
            const auto beginning = high_resolution_clock::now();
            RING_BUFFER::value_type blink{};
            switch(count){
                case 0:
                    std::fill(std::begin(blink),std::end(blink),'*');
                    count++;
                    break;
                case 1:
                    std::fill(std::begin(blink),std::end(blink),'_');
                    count++;
                    break;
                case 2:
                    std::fill(std::begin(blink),std::end(blink),'_');
                    count=0;
                    break;
            }
            PieceWriter<decltype(hostmemory)>(ringbufferbus,blink,333);
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

    unsigned int count = 0;
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
                std::cout << (*print++).at(0);
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