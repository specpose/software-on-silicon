#pragma once

#include "software-on-silicon/EventLoop.hpp"
#include <iostream>

namespace SOS {
    namespace MemoryView {
        class RingBufferIndices : public SOS::MemoryView::TypedWire<size_t,size_t> {
            public:
            enum class FieldName : int{
                Current,
                ThreadCurrent
            };
            RingBufferIndices(size_t current,size_t threadcurrent) : SOS::MemoryView::TypedWire<size_t,size_t>{current,threadcurrent} {}
        };
        template<RingBufferIndices::FieldName index> auto& get(RingBufferIndices& signal){
            return std::get<(int)index>(signal);
        };
    }
class RingBuffer : public SOS::Behavior::EventLoop {
    public:
    RingBuffer(SOS::MemoryView::BusShaker<RingBuffer>& bus) :
            SOS::Behavior::EventLoop(bus.signal), _intrinsic(bus)
            {
    }
    virtual ~RingBuffer(){}
    private:
    SOS::MemoryView::BusShaker<RingBuffer>& _intrinsic;
};
}