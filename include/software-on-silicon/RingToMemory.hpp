/*
    This class is for reading from a driver or gpio on a ControllerHost into a RingBuffer and providing random random
    memory access from either the ControllerHost, or a fpga SubController propped onto the Reader

    ControllerHost<Writer<Reader<SubController>>>

    It is not suitable for reading from a FPGA gpio

    ControllerHost<Reader<Writer<GPIO>>>
*/

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
        template<typename MemoryControllerType> struct BlockerBus{
            using signal_type = SOS::MemoryView::Notify;
            using _arithmetic_type = typename MemoryControllerType::iterator;
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
        template<typename ReaderBusType, typename BlockerBusType> class ReadTask {
            public:
            using reader_length_ct = typename std::tuple_element<0,typename ReaderBusType::const_cables_type>::type;
            using reader_offset_ct = typename std::tuple_element<0,typename ReaderBusType::cables_type>::type;
            ReadTask(reader_length_ct& Length,reader_offset_ct& Offset,BlockerBusType& blockerbus) : _length(Length),_offset(Offset), _blocked(blockerbus) {}
            protected:
            void read(){
                auto current = _length.getReadBufferStartRef();
                const auto end = _length.getReadBufferAfterLastRef();
                const auto readOffset = _offset.getReadOffsetRef().load();
                if (std::distance(std::get<0>(_blocked.const_cables).getBKStartRef(),std::get<0>(_blocked.const_cables).getBKEndRef())
                <(std::distance(current,end)+readOffset))
                    throw SFA::util::runtime_error("Read index out of bounds",__FILE__,__func__);
                std::get<0>(_blocked.cables).getBKReaderPosRef().store(
                        std::get<0>(_blocked.const_cables).getBKStartRef()
                        +readOffset
                        );
                while (current!=end){
                    if (!_blocked.signal.getNotifyRef().test_and_set()) {
                        _blocked.signal.getNotifyRef().clear();
                    } else {
                        auto rP = std::get<0>(_blocked.cables).getBKReaderPosRef().load();
                        *current = *rP;
                        std::get<0>(_blocked.cables).getBKReaderPosRef().store(++rP);
                        ++current;
                    }
                }
            }
            private:
            reader_length_ct& _length;
            reader_offset_ct& _offset;
            BlockerBusType& _blocked;
        };
        template<typename ReaderBusType> class Reader : public SOS::Behavior::EventLoop<SOS::Behavior::SubController> {
            public:
            using bus_type = ReaderBusType;
            Reader(typename bus_type::signal_type& outsideSignal) :
            SOS::Behavior::EventLoop<SOS::Behavior::SubController>(outsideSignal){};
            void event_loop(){};
        };
        template <typename RingBufferBusType>class RingBufferTask {
            public:
            using cable_type = typename std::tuple_element<0,typename RingBufferBusType::cables_type>::type;
            using const_cable_type = typename std::tuple_element<0,typename RingBufferBusType::const_cables_type>::type;
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
        template<typename BufferType> class MemoryControllerWrite {
            public:
            MemoryControllerWrite() {}
            virtual ~MemoryControllerWrite(){};
            protected:
            template<typename T> void write(T WORD);
            //only required for external dependency, has to be called at least once in superclass constructor
            virtual void resize(typename BufferType::difference_type newsize){
                throw SFA::util::logic_error("Memory Allocation is not allowed",__FILE__,__func__);
            };
            protected:
            BufferType memorycontroller = BufferType{};
        };
        template<typename BufferType, typename BlockerBusType> class WriteTask : public SOS::Behavior::MemoryControllerWrite<BufferType> {
            public:
            WriteTask() :
            SOS::Behavior::MemoryControllerWrite<BufferType>{} {
                std::get<0>(_blocker.cables).getBKPosRef().store(std::get<0>(_blocker.const_cables).getBKStartRef());
                std::get<0>(_blocker.cables).getBKReaderPosRef().store(std::get<0>(_blocker.const_cables).getBKEndRef());
            }
            protected:
            void write(const char character) {
                auto pos = std::get<0>(_blocker.cables).getBKPosRef().load();
                if (pos!=std::get<0>(_blocker.const_cables).getBKEndRef()) {
                    *(pos++)=character;
                    std::get<0>(_blocker.cables).getBKPosRef().store(pos);
                } else {
                    throw SFA::util::logic_error("Writer Buffer full",__FILE__,__func__);
                }
            }
            BlockerBusType _blocker = BlockerBusType(this->memorycontroller.begin(),this->memorycontroller.end());
        };
    }
}