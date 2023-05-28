#include "software-on-silicon/RingBuffer.hpp"

int main(){
    auto burp = std::tuple<bool>{false};
    auto test = SOS::MemoryView::TypedWires<bool>{false};
    auto gurp = std::tuple<bool,size_t,size_t>{false,0,0};
    auto myWires = SOS::MemoryView::TypedWires<bool,size_t,size_t>{false,0,0};
    auto buffer = SOS::RingBuffer(myWires);
}