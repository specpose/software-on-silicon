#pragma once

#include "software-on-silicon/EventLoop.hpp"
#include <iostream>

namespace SOS {
    namespace MemoryView {
        enum class RingBufferTaskCableWireName : int{
            Current,
            ThreadCurrent
        };
        template<typename ArithmeticType> struct RingBufferTaskCable : public SOS::Behavior::TaskCable<ArithmeticType, ArithmeticType> {
            RingBufferTaskCable(ArithmeticType current, ArithmeticType threadcurrent, const ArithmeticType end) :
            SOS::Behavior::TaskCable< ArithmeticType, ArithmeticType > {current,threadcurrent},
            end(end)
            {

            }
            const ArithmeticType end;
        };
        template<RingBufferTaskCableWireName index, typename ArithmeticType> auto& get(RingBufferTaskCable<ArithmeticType>& cable){
            return std::get<(int)index>(cable);
        };
        template<typename ArithmeticType> struct TaskBus : public SOS::MemoryView::BusNotifier<SOS::Behavior::SimpleLoop> {
            public:
            using arithmetic_type = ArithmeticType;
        };
    }
    template<typename ArithmeticType> class RingBufferLoop : public SOS::Behavior::SimpleLoop {
        public:
        using arithmetic_type = ArithmeticType;
        RingBufferLoop(SOS::MemoryView::BusNotifier<SOS::Behavior::SimpleLoop>::signal_type& signal) :
                SOS::Behavior::SimpleLoop(signal)
                {
        }
        virtual ~RingBufferLoop() override {};
    };
}