#include "MemoryController.cpp"

class Functor {
    public:
    Functor(SOS::MemoryView::ReaderBus<BLOCK>& readerBus) : _readerBus(readerBus), controller(readerBus) {}
    void asyncRead(BLOCK& buffer, const std::size_t offset){
        _readerBus.setReadBuffer(buffer);
        _readerBus.setOffset(offset);//FIFO has to be called before each getUpdatedRef().clear();
        _readerBus.signal.getUpdatedRef().clear();
    }
    bool operator()(){
        if(!_readerBus.signal.getAcknowledgeRef().test_and_set()){
            return false;
        } else {
            return true;
        }
    }
    private:
    SOS::MemoryView::ReaderBus<BLOCK>& _readerBus;
    WritePriorityImpl controller;
};

using namespace std::chrono;

int main(){
    const std::size_t ara_offset=6992;
    BLOCK randomread{};
    //initialize entire block to zero on all channels
    for (std::size_t i = 0; i < std::size(randomread); i++)
        for (std::size_t j = 0; j < BLOCK::value_type::num_channels; j++)
            randomread[i].channels[j] = BLOCK::value_type::sample_type(0);
    SOS::MemoryView::ReaderBus<BLOCK> readerBus{};
    std::cout << "Writer writing "<<STORAGE_SIZE<<" times from start at rate 1/ms..." << std::endl;
    Functor functor{readerBus};
    std::cout << "Reader reading "<<std::tuple_size<BLOCK>{}<<" times at position "<<ara_offset<<" of memory at rate 1/s..." << std::endl;
    functor.asyncRead(randomread,ara_offset);

    auto loopstart = high_resolution_clock::now();
    auto start_tp = loopstart;
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<11){
        if (functor() && (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start_tp).count() > 0)) {
            start_tp = high_resolution_clock::now();
            for (std::size_t i=0;i<std::tuple_size<BLOCK>{};i++)
                std::cout<< randomread[i].channels[4];
            std::cout<<std::endl;
            functor.asyncRead(randomread,ara_offset);
        }
        std::this_thread::yield();
    }
}