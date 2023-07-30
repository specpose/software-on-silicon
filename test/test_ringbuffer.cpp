#include "RingBuffer.cpp"
#include <iostream>
#include <chrono>

namespace SOSFloat {
class Functor {
    public:
    Functor() {}
    void operator()(){
        //for(int i=0;i<32;i++)
        //    std::cout<<"=";
        hostwriter.writePiece('+',32);
    }
    private:
    RING_BUFFER hostmemory = RING_BUFFER{};
    RingBufferBus<RING_BUFFER> bus{hostmemory.begin(),hostmemory.end()};
    PieceWriter<decltype(hostmemory)> hostwriter{bus};
    RingBufferImpl buffer{bus};
};
}

using namespace std::chrono;

int main(){
    auto functor = SOSFloat::Functor();
    auto loopstart = high_resolution_clock::now();
    while (duration_cast<seconds>(high_resolution_clock::now()-loopstart).count()<10) {
        const auto beginning = high_resolution_clock::now();
        functor();
        std::this_thread::sleep_until(beginning + duration_cast<high_resolution_clock::duration>(milliseconds{1}));
    }
    std::cout<<std::endl;
}