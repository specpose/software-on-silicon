#include <thread>
#include "software-on-silicon/alsa_helpers.hpp"
#include "MemoryControllerAudio.cpp"

class Functor {
public:
    Functor(SOS::MemoryView::ReaderBus<BLOCK>& readerBus)
        : _readerBus(readerBus)
        , controller(_readerBus)
    {
    }
    void asyncRead(BLOCK& buffer, const std::size_t offset)
    {
        _readerBus.setReadBuffer(buffer);
        _readerBus.setOffset(offset); // FIFO has to be called before each getUpdatedRef().clear();
        _readerBus.signal.getUpdatedRef().clear();
    }
    bool operator()()
    {
        if (!_readerBus.signal.getAcknowledgeRef().test_and_set())
            return true;
        else
            return false;
    }

private:
    SOS::MemoryView::ReaderBus<BLOCK>& _readerBus;
    WritePriorityImpl2 controller;
};

using namespace std::chrono;

int main()
{
    const std::size_t ara_offset = 6992 * SAMPLE_RATE / 960;
    BLOCK randomread {};
    // initialize entire block to zero on all channels
    for (std::size_t i = 0; i < std::tuple_size<BLOCK> {}; i++)
        for (std::size_t j = 0; j < BLOCK::value_type::num_channels; j++)
            randomread[i].channels[j] = BLOCK::value_type::sample_type(0);
    SOS::MemoryView::ReaderBus<BLOCK> readerBus {};
    std::cout << "Writer writing " << STORAGE_SIZE << " times from start at rate 1/ms..." << std::endl;
    Functor functor { readerBus };
    std::cout << "Reader reading " << std::tuple_size<BLOCK> {} << " times from offset " << ara_offset << " of memory at rate 1/s..." << std::endl;
    functor.asyncRead(randomread, ara_offset);

    const auto loopstart = high_resolution_clock::now();
    auto beginning = loopstart;
    while (duration_cast<seconds>(high_resolution_clock::now() - loopstart).count() < 11) {
        if (duration_cast<seconds>(high_resolution_clock::now() - beginning).count() > 0) {
            if (functor()) {
                beginning = high_resolution_clock::now();
                for (std::size_t i = 0; i < std::tuple_size<BLOCK> {}; i += SAMPLE_RATE / 960)
                    std::cout << randomread[i].channels[1];
                std::cout << std::endl;
                functor.asyncRead(randomread, ara_offset);
            }
        }
        std::this_thread::yield();
    }
}