#include "software-on-silicon/EventLoop.hpp"
#include <iostream>

namespace SOS {
    namespace MemoryView {
        class Update : public SOS::MemoryView::Signals<1> {
            public:
            enum {
                updated
            };
        };
        //construct the TypedWires from the Signals
        class TypedWireImpl : public SOS::MemoryView::TypedWire<size_t,2> {
            public:
            enum {
                Current,
                ThreadCurrent
            };
        };
    }
class RingBuffer : public SOS::Behavior::EventLoop<SOS::MemoryView::Update> {
    public:
    RingBuffer(SOS::Behavior::EventLoop<SOS::MemoryView::Update>::SignalType& signalbus,
    SOS::MemoryView::TypedWireImpl& databus) : 
                                    SOS::Behavior::EventLoop<SOS::MemoryView::Update>(signalbus),
                                    _foreignData(databus)
                                    {
    }
    virtual ~RingBuffer(){}
    virtual void eventloop(){};
    //Indexes should match in signals and wires!
    protected:
    template<size_t SignalNumber> static void task(SOS::MemoryView::TypedWireImpl& wire){}
    SOS::MemoryView::TypedWireImpl& _foreignData;
};
}