#include "MemoryController.cpp"

namespace SOSFloat {
class Functor {
    public:
    Functor(bool start = false){
        readerBus.setReadBuffer(randomread);
        if (start) {
            readerBus.setOffset(7991);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
        }
    }
    void operator()(int offset){
        if(!readerBus.signal.getAcknowledgeRef().test_and_set()){
            readerBus.setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear()
            readerBus.signal.getUpdatedRef().clear();
            auto print = randomread[4].begin();//HACK: hard-coded channel 5
            while (print!=randomread[4].end())//HACK: hard-coded channel 5
                std::cout << (*print++);
            std::cout << std::endl;
        }
    }
    private:
    READ_BUFFER randomread = READ_BUFFER{};
    ReaderBus<READ_BUFFER> readerBus{};
    WritePriorityImpl controller{readerBus};
};
}

int main(){
    std::cout << "Writer writing 10000 times from start at rate 1/ms..." << std::endl;
    auto functor = SOSFloat::Functor(true);
    std::cout << "Reader reading 1000 times at tail of memory at rate 1/s..." << std::endl;
    while (true){
        const auto start_tp = high_resolution_clock::now();
        functor(7991);
        std::this_thread::sleep_until(start_tp + duration_cast<high_resolution_clock::duration>(seconds{1}));
    }
}