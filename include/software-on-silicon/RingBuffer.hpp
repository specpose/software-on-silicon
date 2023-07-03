#pragma once

#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/error.hpp"

namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct WriteSize : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            WriteSize(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
            enum class wire_names : unsigned char { startPos, afterLast };
        };
        template<typename ArithmeticType, typename WriteSize<ArithmeticType>::wire_names index> auto& get(WriteSize<ArithmeticType>& cable){
            return std::get<(unsigned char)index>(cable);
        };
        template<typename ArithmeticType> struct WriteLength : public SOS::MemoryView::TaskCable<ArithmeticType,1> {
            using SOS::MemoryView::TaskCable<ArithmeticType,1>::TaskCable;
            enum class wire_names : unsigned char{ WriteLength } ;
        };
        template<typename ArithmeticType, typename WriteLength<ArithmeticType>::wire_names index> auto& get(WriteLength<ArithmeticType>& cable){
            return std::get<(unsigned char)index>(cable);
        };
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
            using _pointer_type = std::array<char,0>::iterator;
            using _difference_type = std::array<char,0>::difference_type;
            using cables_type = std::tuple< SOS::MemoryView::RingBufferTaskCable<_pointer_type>,WriteLength<_difference_type> >;
            using const_cables_type = std::tuple< WriteSize<_pointer_type> >;
            RingBufferBus(const _pointer_type begin, const _pointer_type afterlast) :
            //tuple requires copy constructor for any tuple that isn't default constructed
            //cables{std::tuple< RingBufferTaskCable<_pointer_type>,WriteLength<_difference_type> >{}
            //},
            const_cables(
            std::tuple< WriteSize<_pointer_type> >{WriteSize<_pointer_type>(begin,afterlast)}
            )//,
            //start(begin),
            //end(afterlast)
            {
                if(std::distance(begin,afterlast)<2)
                    throw SFA::util::logic_error("Requested RingBuffer size not big enough.",__FILE__,__func__);
                //=>explicitly initialize wires
                setLength(1);
                get<_pointer_type,RingBufferTaskCable<_pointer_type>::wire_names::ThreadCurrent>(std::get<0>(cables)).store(begin);
                auto next = begin;
                get<_pointer_type,RingBufferTaskCable<_pointer_type>::wire_names::Current>(std::get<0>(cables)).store(++next);
            }
            void setLength (_difference_type length){
                get<_difference_type,WriteLength<_difference_type>::wire_names::WriteLength>(std::get<1>(cables)).store(length);
            }
            signal_type signal;
            cables_type cables;
            const_cables_type const_cables;
            //const _pointer_type start,end;
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