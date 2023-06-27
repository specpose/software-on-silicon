#include "software-on-silicon/Controller.hpp"

namespace SOS{
    namespace MemoryView{
        template<typename ArithmeticType> struct ReaderCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            using wire_names = enum indices : unsigned char{ Start, AfterLast} ;
        };
        //memorycontroller =>
        template<typename OutputBufferTypeIterator> struct ReaderBus {
            using signal_type = SOS::MemoryView::bus_traits<SOS::MemoryView::BusNotifier>::signal_type;
            using cables_type = std::tuple< SOS::MemoryView::TaskCable<OutputBufferTypeIterator,2> >;
            signal_type signal;
            cables_type cables;
        };
    }
    namespace Behavior{
        class Reader;
        template<> struct task_traits<Reader> {
            using cable_type = SOS::MemoryView::TaskCable<std::array<double,0>::iterator,2>;
        };
        class Reader : public SOS::Behavior::SimpleLoop, protected SOS::Behavior::Task {
            public:
            //from SimpleLoop
            using bus_type = SOS::MemoryView::ReaderBus<SOS::Behavior::task_traits<SOS::Behavior::Reader>::cable_type::cable_arithmetic>;
            //from Task
            using cable_type = typename SOS::Behavior::task_traits<Reader>::cable_type;
            Reader(bus_type& outside) : SOS::Behavior::SimpleLoop(outside.signal) {};
            void event_loop(){};
        };
        class WritePriority : private SOS::Behavior::Remote<Reader> {
            public:
            WritePriority(
                typename SOS::Behavior::Remote<Reader>::bus_type& myBus,
                typename SOS::Behavior::Remote<Reader>::subcontroller_type::bus_type& passThru
                ) : Remote<Reader>(myBus.signal, passThru) {};
            virtual ~WritePriority(){};
            void event_loop(){};
        };
    }
}