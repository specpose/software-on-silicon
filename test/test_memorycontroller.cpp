#include "MemoryController.cpp"

class Functor {
    public:
    Functor(bool start = false){
        if (start) {
            readerBus.setOffset(8000);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
        }
    }
    void operator()(int offset){
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
            auto print = randomread.begin();
            while (print!=randomread.end())
                std::cout << *print++;
            std::cout << std::endl;
        }
    }
    private:
    READ_BUFFER randomread = READ_BUFFER{};
    ReaderBus<READ_BUFFER> readerBus{randomread.begin(),randomread.end()};
    WritePriorityImpl controller{readerBus};
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