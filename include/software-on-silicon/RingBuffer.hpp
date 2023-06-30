#pragma once

#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/error.hpp"

namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct RingBufferTaskCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            enum class wire_names : unsigned char{ Current, ThreadCurrent} ;
        };
        template<typename ArithmeticType, typename RingBufferTaskCable<ArithmeticType>::wire_names index> auto& get(
            RingBufferTaskCable<ArithmeticType>& cable
            ){
            return std::get<(unsigned char)index>(cable);
        };
        struct RingBufferBus {
            using signal_type = bus_traits<SOS::MemoryView::BusNotifier>::signal_type;
            using _arithmetic_type = std::array<char,0>::iterator;
            using cables_type = std::tuple< SOS::MemoryView::RingBufferTaskCable<_arithmetic_type> >;
            RingBufferBus(_arithmetic_type begin, _arithmetic_type afterlast) :
            //tuple requires copy constructor for any tuple that isn't default constructed
            cables{std::tuple< RingBufferTaskCable<_arithmetic_type> >{}
            },
            start(begin),
            end(afterlast)
            {
                //explicitly initialize wires
                get<_arithmetic_type,RingBufferTaskCable<_arithmetic_type>::wire_names::ThreadCurrent>(std::get<0>(cables)).store(begin);
                if(std::distance(start,end)<2)
                    throw SFA::util::logic_error("Requested RingBuffer size not big enough.",__FILE__,__func__);
                auto next = start;
                get<_arithmetic_type,RingBufferTaskCable<_arithmetic_type>::wire_names::Current>(std::get<0>(cables)).store(++next);
            }
            signal_type signal;
            cables_type cables;
            const _arithmetic_type start,end;
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