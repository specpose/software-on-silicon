#include "software-on-silicon/RingBuffer.hpp"

int main(){
    auto burp = std::tuple<bool>{false};
    auto test = SOS::MemoryView::TypedWires<bool>{false};
    auto gurp = std::tuple<bool,size_t,size_t>{false,0,0};
    auto mySignals = SOS::MemoryView::Signals<2>{std::atomic_flag{},std::atomic_flag{}};
    auto buffer = SOS::RingBuffer(mySignals);
}