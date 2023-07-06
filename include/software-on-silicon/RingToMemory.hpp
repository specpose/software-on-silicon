#pragma once
#include "software-on-silicon/EventLoop.hpp"
#include "software-on-silicon/error.hpp"

namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct BlockerCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            auto& getBKReaderPosRef(){return std::get<0>(*this);}
            auto& getBKPosRef(){return std::get<1>(*this);}
        };
        //this is not const_cable because of external dependency
        template<typename ArithmeticType> struct MemoryControllerBufferSize : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            //using SOS::MemoryView::ConstCable<ArithmeticType, 2>::ConstCable;
            MemoryControllerBufferSize(const ArithmeticType& start, const ArithmeticType& end): SOS::MemoryView::ConstCable<ArithmeticType,2>{start,end} {}
            auto& getBKStartRef(){return std::get<0>(*this);}
            auto& getBKEndRef(){return std::get<1>(*this);}
        };
        struct BlockerBus{
            using signal_type = SOS::MemoryView::Notify;
            using _arithmetic_type = std::array<char,0>::iterator;
            using cables_type = std::tuple< BlockerCable<_arithmetic_type> >;
            using const_cables_type = std::tuple< MemoryControllerBufferSize<_arithmetic_type> >;
            BlockerBus(const _arithmetic_type start, const _arithmetic_type end) :
            //tuple requires copy constructor for any tuple that isn't default constructed
            const_cables(
            std::tuple< MemoryControllerBufferSize<_arithmetic_type> >{MemoryControllerBufferSize<_arithmetic_type>(start,end)}
            ){}
            signal_type signal;
            cables_type cables;
            const_cables_type const_cables;
        };
    }
    namespace Behavior {
        template<typename BufferType> class MemoryControllerWrite {
            public:
            using blocker_ct = std::tuple_element<0,SOS::MemoryView::BlockerBus::cables_type>::type;
            using blocker_buffer_size = std::tuple_element<0,SOS::MemoryView::BlockerBus::const_cables_type>::type;
            MemoryControllerWrite() {}
            virtual ~MemoryControllerWrite(){};
            protected:
            template<typename T> void write(T WORD);
            //required for external dependency, has to be called in superclass constructor
            virtual void resize(typename BufferType::difference_type newsize){
                throw SFA::util::logic_error("Memory Allocation is not allowed",__FILE__,__func__);
            };
            protected:
            BufferType memorycontroller = BufferType{};
        };
    }
}