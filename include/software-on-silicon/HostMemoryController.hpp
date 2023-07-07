#include "software-on-silicon/RingToMemory.hpp"
#include "software-on-silicon/error.hpp"

namespace SOS{
    namespace MemoryView{
        template<typename ArithmeticType> struct ReadLength : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            ReadLength(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
            auto& getReadBufferStartRef(){return std::get<0>(*this);}
            auto& getReadBufferAfterLastRef(){return std::get<1>(*this);}
        };
        template<typename ArithmeticType> struct ReadOffset : public SOS::MemoryView::TaskCable<ArithmeticType,1> {
            using SOS::MemoryView::TaskCable<ArithmeticType,1>::TaskCable;
            auto& getReadOffsetRef(){return std::get<0>(*this);}
        };
        template<typename OutputBuffer> struct ReaderBus : public SOS::MemoryView::BusShaker {
            //using signal_type = typename SOS::MemoryView::BusShaker::signal_type;
            using _pointer_type = typename OutputBuffer::iterator;
            using _difference_type = typename OutputBuffer::difference_type;
            using cables_type = std::tuple< ReadOffset<_difference_type> >;
            using const_cables_type = std::tuple< ReadLength<_pointer_type> >;
            ReaderBus(const _pointer_type begin, const _pointer_type end)
            : const_cables(
                std::tuple< ReadLength<_pointer_type> >{ReadLength<_pointer_type>(begin,end)}
                )
            {
                if (std::distance(begin,end)<0)
                    throw SFA::util::runtime_error("Invalid Read Destination",__FILE__,__func__);
                setOffset(0);
            }
            void setOffset(_difference_type offset){
                std::get<0>(cables).getReadOffsetRef().store(offset);
            }
            //signal_type signal;
            cables_type cables;
            const_cables_type const_cables;
        };
    }
}