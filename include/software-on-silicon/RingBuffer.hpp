#pragma once

#include "software-on-silicon/RingToMemory.hpp"
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
        template<typename ArithmeticType> struct SamplePosition : public SOS::MemoryView::TaskCable<ArithmeticType,1> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 1>::TaskCable;
            auto& getSamplePositionRef(){return std::get<0>(*this);}
        };
        struct RingBufferBus {
            using signal_type = bus_traits<SOS::MemoryView::BusNotifier>::signal_type;
            using _pointer_type = std::array<char,0>::iterator;
            using _difference_type = std::array<char,0>::difference_type;
            using cables_type = std::tuple< SOS::MemoryView::RingBufferTaskCable<_pointer_type>,WriteLength<_difference_type>,SamplePosition<_difference_type> >;
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
                setSamplePosition(0);//not used with continuous sources
                setLength(1);
                std::get<0>(cables).getThreadCurrentRef().store(begin);
                auto next = begin;
                std::get<0>(cables).getCurrentRef().store(++next);
            }
            void setSamplePosition(_difference_type position){
                    std::get<2>(cables).getSamplePositionRef().store(position);
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
    namespace Behavior {
        class RingBufferTask {
            public:
            using cable_type = std::tuple_element<0,SOS::MemoryView::RingBufferBus::cables_type>::type;
            using const_cable_type = std::tuple_element<0,SOS::MemoryView::RingBufferBus::const_cables_type>::type;
            RingBufferTask(cable_type& indices, const_cable_type& bounds) :
            _item(indices),
            _bounds(bounds)
            {}
            void read_loop() {
                auto threadcurrent = _item.getThreadCurrentRef().load();
                auto current = _item.getCurrentRef().load();
                bool stop = false;
                while(!stop){//if: possible less writes than reads
                    ++threadcurrent;
                    if (threadcurrent==_bounds.getWriterEndRef())
                        threadcurrent=_bounds.getWriterStartRef();
                    if (threadcurrent!=current) {
                        write(*threadcurrent);
                        _item.getThreadCurrentRef().store(threadcurrent);
                    } else {
                        stop = true;
                    }
                }
            }
            protected:
            virtual void write(const char character)=0;
            private:
            cable_type& _item;
            const_cable_type& _bounds;
        };
    }
}