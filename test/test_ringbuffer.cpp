#include "RingBuffer.cpp"
#include "software-on-silicon/ringbuffer_helpers.hpp"

using BLINK_T = std::array<RING_BUFFER::value_type, 9>;

class Functor {
public:
    Functor() { }
    void operator()(BLINK_T& piece)
    {
        for (std::size_t i = 0; i < std::tuple_size<RING_BUFFER> {} - 1; i++)
            WriteInterleaved<decltype(hostmemory), BLINK_T::value_type>(bus, piece[i]);
    }

private:
    RING_BUFFER hostmemory = RING_BUFFER {};
    SOS::MemoryView::RingBufferBus<RING_BUFFER> bus { hostmemory.begin(), hostmemory.end() };
    RingBufferImpl buffer { bus };
};

using namespace std::chrono;

int main()
{
    BLINK_T channelBuffers32 {};
    // initialise channel of sample to its channel number
    for (std::size_t i = 0; i < std::tuple_size<BLINK_T> {}; i++)
        for (std::size_t j = 0; j < std::tuple_size<RING_BUFFER::value_type> {}; j++)
            channelBuffers32[i][j].channels[0] = RING_BUFFER::value_type::value_type::sample_type(i);
    Functor functor {};
    auto loopstart = high_resolution_clock::now();
    auto beginning = loopstart;
    while (duration_cast<seconds>(high_resolution_clock::now() - loopstart).count() < 10) {
        if (duration_cast<milliseconds>(high_resolution_clock::now() - beginning).count() > 0) {
            beginning = high_resolution_clock::now();
            functor(channelBuffers32);
        }
        std::this_thread::yield();
    }
    std::cout << std::endl;
}