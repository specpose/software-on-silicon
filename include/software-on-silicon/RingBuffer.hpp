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
class RingBuffer : public SOS::Behavior::SimpleLoop {
    public:
    RingBuffer(SOS::MemoryView::BusNotifier<RingBuffer>& bus) :
            SOS::Behavior::SimpleLoop(bus.signal), _intrinsic(bus)
            {
    }
    virtual ~RingBuffer() override {};
    private:
    SOS::MemoryView::BusNotifier<RingBuffer>& _intrinsic;
};
}