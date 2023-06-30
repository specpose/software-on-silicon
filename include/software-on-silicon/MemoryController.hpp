#include "software-on-silicon/Controller.hpp"
#include <chrono>
#include "stackable-functor-allocation/error.h"

using namespace std::chrono;

namespace SOS{
    namespace MemoryView{
        struct ReadLength : public SOS::MemoryView::TaskCable<std::array<char,0>::iterator,2> {
            using SOS::MemoryView::TaskCable<std::array<char,0>::iterator, 2>::TaskCable;
            enum class wire_names : unsigned char { startPos, afterLast };
        };
        template<typename ReadLength::wire_names index> auto& get(ReadLength& cable){
            return std::get<(unsigned char)index>(cable);
        };
        struct ReadOffset : public SOS::MemoryView::TaskCable<std::array<char,0>::difference_type,1> {
            using SOS::MemoryView::TaskCable<std::array<char,0>::difference_type,1>::TaskCable;
            enum class wire_names : unsigned char{ readOffset } ;
        };
        template<typename ReadOffset::wire_names index> auto& get(ReadOffset& cable){
            return std::get<(unsigned char)index>(cable);
        };
        struct ReaderBus {
            using signal_type = SOS::MemoryView::bus_traits<SOS::MemoryView::BusShaker>::signal_type;
            using cables_type = std::tuple< ReadLength, ReadOffset >;
            ReaderBus(std::array<char,0>::iterator begin, std::array<char,0>::iterator end) :
            start(begin),
            end(end){
                get<ReadLength::wire_names::startPos>(get<0>(cables)).store(begin);
                if (std::distance(start,end)<0)
                    throw SFA::util::runtime_error("Invalid Read Destination",__FILE__,__func__);
                get<ReadLength::wire_names::afterLast>(get<0>(cables)).store(end);
            }
            void setOffset(std::array<char,0>::difference_type offset){
                get<ReadOffset::wire_names::readOffset>(std::get<1>(cables)).store(offset);
            }
            signal_type signal;
            cables_type cables;
            std::array<char,0>::iterator start,end;
        };
        struct BlockerCable : public SOS::MemoryView::TaskCable<std::array<char,0>::iterator,2> {
            using SOS::MemoryView::TaskCable<std::array<char,0>::iterator, 2>::TaskCable;
            enum class wire_names : unsigned char{ readerPos, pos } ;
        };
        template<typename SOS::MemoryView::BlockerCable::wire_names index> auto& get(
            SOS::MemoryView::BlockerCable& cable
            ){
            return std::get<(unsigned char)index>(cable);
        };
        struct BlockerBus{
            using signal_type = SOS::MemoryView::Notify;
            using cables_type = std::tuple< BlockerCable >;
            signal_type signal;
            cables_type cables;
            std::array<char,0>::iterator start,end;
        };
    }
    namespace Behavior{
        /*class Reader;
        template<> struct task_traits<Reader> {
            using cable_type = SOS::MemoryView::TaskCable<std::array<double,0>::iterator,2>;
        };*/
        class Reader : public SOS::Behavior::EventLoop<SOS::Behavior::SubController> {//, protected SOS::Behavior::Task {
            public:
            //from SimpleLoop
            using bus_type = SOS::MemoryView::ReaderBus;
            //from Task
            //using cable_type = typename SOS::Behavior::task_traits<Reader>::cable_type;
            Reader(bus_type::signal_type& outsideSignal) :
            SOS::Behavior::EventLoop<SOS::Behavior::SubController>(outsideSignal){};
            void event_loop(){};
        };
        template<typename S> class WritePriority : public Loop {
        public:
        using bus_type = typename SimpleLoop<S>::bus_type;
        using subcontroller_type = S;
        WritePriority(
            typename SOS::Behavior::SimpleLoop<S>::bus_type::signal_type& mySignal,
            typename SOS::Behavior::SimpleLoop<S>::subcontroller_type::bus_type& passThru
            ) : _intrinsic(mySignal), _child(S(passThru,_blocker)) {};
        virtual ~WritePriority(){};
        void event_loop(){}
        protected:
        typename bus_type::signal_type& _intrinsic;
        SOS::MemoryView::BlockerBus _blocker = SOS::MemoryView::BlockerBus{};
        private:
        S _child;
        };
    }
}