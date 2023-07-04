#pragma once

#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/error.hpp"

namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct WriteBufferSize : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            WriteBufferSize(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
            auto& getWriterStartRef(){return std::get<0>(*this);}
            auto& getWriterEndRef(){return std::get<1>(*this);}
        };
        template<typename ArithmeticType> struct WriteLength : public SOS::MemoryView::TaskCable<ArithmeticType,1> {
            using SOS::MemoryView::TaskCable<ArithmeticType,1>::TaskCable;
            auto& getWriteLengthRef(){return std::get<0>(*this);}
        };
        template<typename ArithmeticType> struct RingBufferTaskCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            auto& getCurrentRef(){return std::get<0>(*this);}
            auto& getThreadCurrentRef(){return std::get<1>(*this);}
        };
        struct RingBufferBus {
            using signal_type = bus_traits<SOS::MemoryView::BusNotifier>::signal_type;
            using _pointer_type = std::array<char,0>::iterator;
            using _difference_type = std::array<char,0>::difference_type;
            using cables_type = std::tuple< SOS::MemoryView::RingBufferTaskCable<_pointer_type>,WriteLength<_difference_type> >;
            using const_cables_type = std::tuple< WriteBufferSize<_pointer_type> >;
            RingBufferBus(const _pointer_type begin, const _pointer_type afterlast) :
            //tuple requires copy constructor for any tuple that isn't default constructed
            const_cables(
            std::tuple< WriteBufferSize<_pointer_type> >{WriteBufferSize<_pointer_type>(begin,afterlast)}
            )
            {
                if(std::distance(begin,afterlast)<2)
                    throw SFA::util::logic_error("Requested RingBuffer size not big enough.",__FILE__,__func__);
                //=>explicitly initialize wires
                setLength(1);
                std::get<0>(cables).getThreadCurrentRef().store(begin);
                auto next = begin;
                std::get<0>(cables).getCurrentRef().store(++next);
            }
            void setLength (_difference_type length){
                std::get<1>(cables).getWriteLengthRef().store(length);
            }
            signal_type signal;
            cables_type cables;
            const_cables_type const_cables;
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