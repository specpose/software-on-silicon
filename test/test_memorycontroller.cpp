#include "MemoryController.cpp"

class Functor {
    public:
    Functor(const std::size_t readOffset=0) : _readOffset(readOffset) {
        _readerBus.setOffset(_readOffset);//FIFO has to be called before each getUpdatedRef().clear()
        _readerBus.signal.getUpdatedRef().clear();
    }
    void operator()(){
        if(!_readerBus.signal.getAcknowledgeRef().test_and_set()){
            _readerBus.signal.getUpdatedRef().clear();//offset change omitted
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << (print++)->channels[0];//HACK: hard coded channel 0
            std::cout << std::endl;
        }
    }
    private:
    const std::size_t _readOffset;
    BLOCK randomread{};
    SOS::MemoryView::ReaderBus<decltype(randomread)> _readerBus{randomread.begin(),randomread.end()};
    WritePriorityImpl controller{_readerBus};
};

using namespace std::chrono;

int main(){
    std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
    auto functor = Functor(6992);
    std::cout << "Reader reading 1000 times at tail of memory at rate 1/s..." << std::endl;
    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<11){
        const auto start_tp = high_resolution_clock::now();
        functor();
        std::this_thread::sleep_until(start_tp + duration_cast<high_resolution_clock::duration>(seconds{1}));
    }
}