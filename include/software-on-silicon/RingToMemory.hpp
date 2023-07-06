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
        template<typename ArithmeticType> struct MemoryControllerBufferSize : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            auto& getBKStartRef(){return std::get<0>(*this);}
            auto& getBKEndRef(){return std::get<1>(*this);}
        };
        struct BlockerBus{
            using signal_type = SOS::MemoryView::Notify;
            using _arithmetic_type = std::array<char,0>::iterator;
            using cables_type = std::tuple< BlockerCable<_arithmetic_type>,MemoryControllerBufferSize<_arithmetic_type> >;
            signal_type signal;
            cables_type cables;
        };
    }
    namespace Behavior {
        class MemoryControllerWrite {
            public:
            using blocker_ct = std::tuple_element<0,SOS::MemoryView::BlockerBus::cables_type>::type;
            using blocker_buffer_size = std::tuple_element<1,SOS::MemoryView::BlockerBus::cables_type>::type;
            MemoryControllerWrite(blocker_ct& posBlocker,blocker_buffer_size& bufferSize) : _item(posBlocker),_size(bufferSize) {}
            protected:
            template<typename T> void write(T character);
            //required for external dependency, has to be called in superclass constructor
            virtual void resize(blocker_buffer_size::cable_arithmetic start, blocker_buffer_size::cable_arithmetic end)=0;
            protected:
            blocker_ct& _item;
            blocker_buffer_size& _size;
        };
    }
}