#include "software-on-silicon/TypedWires.hpp"

enum {
    updated,
    next,
    thread_current
};

namespace SOS {
class RingBuffer : private SOS::Behavior::EventLoop<bool,size_t,size_t> {
    public:
    using WireType = SOS::MemoryView::TypedWires<bool,size_t,size_t>;
    RingBuffer(WireType& databus) : SOS::Behavior::EventLoop<bool,size_t,size_t>(databus) {}
    void eventloop(){

    }
    void operator()(){

    }
};
}