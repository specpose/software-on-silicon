#include "software-on-silicon/TypedWires.hpp"

enum {
    updatedNext,
    updatedThreadCurrent
};

namespace SOS {
class RingBuffer : private SOS::Behavior::EventLoop<2> {
    public:
    //using WireType = SOS::MemoryView::TypedWires<bool,size_t,size_t>;
    RingBuffer(SOS::MemoryView::Signals<2>& databus) : SOS::Behavior::EventLoop<2>(databus) {
        //impossible: base class starting before local construction
    }
    void eventloop(){

    }
    void operator()(){

    }
};
}