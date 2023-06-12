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
            private:
            size_t size;
        };
        template<RingBufferIndices::FieldName index> auto& get(RingBufferIndices& signal){
            return std::get<(int)index>(signal);
        };
        template<typename T, size_t WordSize> struct RingBufferData : std::vector<std::array<T,WordSize>>{
            public:
            RingBufferData(size_t n) : std::vector<std::array<T,WordSize>>(n){}
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