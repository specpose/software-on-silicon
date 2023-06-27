#include "software-on-silicon/MemoryController.hpp"
#include "software-on-silicon/RingBuffer.hpp"

int main(){
    auto writerBus = SOS::MemoryView::BusNotifier{};
    auto readerBus = SOS::MemoryView::ReaderBus{};
    auto controller = SOS::Behavior::WritePriority(writerBus,readerBus);
}