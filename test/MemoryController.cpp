#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/RingBuffer.hpp"

int main(){
    auto writerBus = SOS::MemoryView::BusNotifier{};
    using OutputBufferType = std::array<double,10000>;
    auto readerBus = SOS::MemoryView::ReaderBus<OutputBufferType::iterator>{};
    auto controller = SOS::Behavior::WritePriority(writerBus,readerBus);
}