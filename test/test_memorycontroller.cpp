#include "MemoryController.cpp"

class Functor {
    public:
    Functor(bool start = false){
        if (start) {
            _readerBus.setOffset(8000);//FIFO has to be called before each getUpdatedRef().clear()
            _readerBus.signal.getUpdatedRef().clear();
        }
    }
    void operator()(const std::size_t offset){
        if(!_readerBus.signal.getAcknowledgeRef().test_and_set()){
            _readerBus.setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear()
            _readerBus.signal.getUpdatedRef().clear();
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << (print++)->channels[0];//HACK: hard coded channel 0
            std::cout << std::endl;
        }
    }
    private:
    BLOCK randomread{};
    SOS::MemoryView::ReaderBus<decltype(randomread)> _readerBus{randomread.begin(),randomread.end()};
    WritePriorityImpl controller{_readerBus};
};

using namespace std::chrono;

int main(){
    std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
    auto functor = Functor(true);
    std::cout << "Reader reading 1000 times at tail of memory at rate 1/s..." << std::endl;
    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<12){
        const auto start_tp = high_resolution_clock::now();
        functor(8000);
        std::this_thread::sleep_until(start_tp + duration_cast<high_resolution_clock::duration>(seconds{1}));
    }
}