#pragma once

#include "software-on-silicon/EventLoop.hpp"
#include <iostream>

namespace SOS {
    namespace MemoryView {
        class RingBufferIndices : public SOS::MemoryView::TypedWire<std::atomic<size_t>,std::atomic<size_t>> {
            public:
            using SOS::MemoryView::TypedWire<std::atomic<size_t>,std::atomic<size_t>>::TypedWire;
            enum {
                Current,
                ThreadCurrent
            };
        };
    }
class RingBuffer : public SOS::Behavior::EventLoop<
SOS::MemoryView::Bus<SOS::MemoryView::RingBufferIndices,SOS::MemoryView::HandShake>
> {
    public:
    RingBuffer(SOS::Behavior::EventLoop<
    SOS::MemoryView::Bus<SOS::MemoryView::RingBufferIndices,SOS::MemoryView::HandShake>
    >::bus_type& bus) :
            SOS::Behavior::EventLoop<
            SOS::MemoryView::Bus<SOS::MemoryView::RingBufferIndices,SOS::MemoryView::HandShake>
            >(bus)
            {
    }
    virtual ~RingBuffer(){}
};
}