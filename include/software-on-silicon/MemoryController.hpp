#include "software-on-silicon/Controller.hpp"
#include <chrono>

using namespace std::chrono;

namespace SOS{
    namespace MemoryView{
        struct ReaderCable : public SOS::MemoryView::TaskCable<std::array<char,0>::iterator,2> {
            using SOS::MemoryView::TaskCable<std::array<char,0>::iterator, 2>::TaskCable;
            enum class wire_names : unsigned char{ Offset, Length} ;
        };
        template<typename ReaderCable::wire_names index> auto& get(
            ReaderCable& cable
            ){
            return std::get<(unsigned char)index>(cable);
        };
        //memorycontroller =>
        struct ReaderBus {
            using signal_type = SOS::MemoryView::bus_traits<SOS::MemoryView::BusShaker>::signal_type;
            using cables_type = std::tuple< ReaderCable >;
            signal_type signal;
            cables_type cables;
        };
        struct BlockerCable : public SOS::MemoryView::TaskCable<std::array<char,0>::iterator,1> {
            using SOS::MemoryView::TaskCable<std::array<char,0>::iterator, 1>::TaskCable;
            enum class wire_names : unsigned char{ readerPos } ;
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
            Reader(bus_type& outside, SOS::MemoryView::BlockerBus& blockerbus) :
            SOS::Behavior::EventLoop<SOS::Behavior::SubController>(outside.signal),
            _blocked(blockerbus)
            {};
            void event_loop(){};
            protected:
            SOS::MemoryView::BlockerBus& _blocked;
        };
        template<typename S> class WritePriority : public SimpleLoop<S> {
        public:
        WritePriority(
            typename SOS::Behavior::SimpleLoop<S>::bus_type& myBus,
            typename SOS::Behavior::SimpleLoop<S>::subcontroller_type::bus_type& passThru
            ) : SimpleLoop<S>(myBus.signal), _foreign(passThru), _child(S(_foreign,_blocker)) {
            };
        virtual ~WritePriority(){};
        void event_loop(){}
        protected:
        typename SimpleLoop<S>::subcontroller_type::bus_type& _foreign;
        SOS::MemoryView::BlockerBus _blocker = SOS::MemoryView::BlockerBus{};
        private:
        S _child;
        };
    }
}