#pragma once

#include "software-on-silicon/Controller.hpp"
#include <iostream>

namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct RingBufferTaskCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            enum wire_names : unsigned char{ Current, ThreadCurrent} ;
        };
        struct TaskBus {
            using signal_type = bus_traits<SOS::MemoryView::BusNotifier>::signal_type;
            using cables_type = std::tuple< TaskCable<void,0> >;
            signal_type signal;
            cables_type cables;
        };
    }
    class RingBufferLoop : public SOS::Behavior::SimpleLoop<SOS::Behavior::SubController> {
        public:
        RingBufferLoop(SOS::MemoryView::BusNotifier::signal_type& signal) :
                SOS::Behavior::SimpleLoop<SOS::Behavior::SubController>(signal)
                {
        }
        virtual ~RingBufferLoop() override {};
    };
}