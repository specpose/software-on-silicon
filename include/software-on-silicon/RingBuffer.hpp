#pragma once

#include "software-on-silicon/EventLoop.hpp"
#include <iostream>

namespace SOS {
    namespace MemoryView {
        /*enum RingBufferTaskCableWireName : int{
            Current,
            ThreadCurrent
        };*/
        template<typename ArithmeticType> struct RingBufferTaskCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            using wire_names = enum indices : unsigned char{ Current, ThreadCurrent} ;
        };
        /*template<RingBufferTaskCableWireName index, typename ArithmeticType> auto& get(RingBufferTaskCable<ArithmeticType>& cable){
            return std::get<static_cast<int>(index)>(cable);
        };*/
        struct TaskBus {
            using signal_type = bus_traits<SOS::MemoryView::BusNotifier>::signal_type;
            using cables_type = std::tuple< TaskCable<void,0> >;
            signal_type signal;
            cables_type cables;
        };
    }
    class RingBufferLoop : public SOS::Behavior::SimpleLoop {
        public:
        RingBufferLoop(SOS::MemoryView::BusNotifier::signal_type& signal) :
                SOS::Behavior::SimpleLoop(signal)
                {
        }
        virtual ~RingBufferLoop() override {};
    };
}