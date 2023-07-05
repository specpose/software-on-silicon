#include "software-on-silicon/EventLoop.hpp"

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
            virtual void write(const char character)=0;
        };
    }
}