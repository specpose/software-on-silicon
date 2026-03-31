#define STORAGE_SIZE 10000
#define BLOCK_SIZE 1000
#define SAMPLE_TYPE char
#define NUM_CHANNELS 1
#include "MemoryController.cpp"

class Functor {
    public:
    Functor(const std::size_t readOffset=0) : _readOffset(readOffset), randomread(), _readerBus(randomread.begin(),randomread.end()), controller(_readerBus) {
        _readerBus.setOffset(_readOffset);
        _readerBus.signal.getUpdatedRef().clear();
    }
    void operator()(){
        if(!_readerBus.signal.getAcknowledgeRef().test_and_set()){
            _readerBus.signal.getUpdatedRef().clear();//offset change omitted
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << (*(print++))[0];//HACK: hard coded channel 0
            std::cout << std::endl;
        }
    }
    private:
    const std::size_t _readOffset;
    BLOCK randomread;
    SOS::MemoryView::ReaderBus<decltype(randomread)> _readerBus;
    WritePriorityImpl controller;
};

using namespace std::chrono;

int main(){
    const std::size_t ara_offset = 6992;
    std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
    Functor functor(ara_offset);
    std::cout << "Reader reading 1000 times from offset "<<ara_offset<<" of memory at rate 1/s..." << std::endl;
    auto loopstart = high_resolution_clock::now();
    auto start_tp = loopstart;
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<11){
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start_tp).count() > 0) {
        start_tp = high_resolution_clock::now();
        functor();
        }
        std::this_thread::yield();
    }
}