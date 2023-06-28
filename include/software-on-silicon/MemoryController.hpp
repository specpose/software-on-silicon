#include "software-on-silicon/Controller.hpp"

namespace SOS{
    namespace MemoryView{
        template<typename ArithmeticType> struct ReaderCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            enum class wire_names : unsigned char{ Start, AfterLast} ;
        };
        template<typename ArithmeticType, typename ReaderCable<ArithmeticType>::wire_names index> auto& get(
            ReaderCable<ArithmeticType>& cable
            ){
            return std::get<(unsigned char)index>(cable);
        };
        //memorycontroller =>
        struct ReaderBus {
            using signal_type = SOS::MemoryView::bus_traits<SOS::MemoryView::BusNotifier>::signal_type;
            using cables_type = std::tuple< SOS::MemoryView::TaskCable<std::array<double,0>::iterator,2> >;
            signal_type signal;
            cables_type cables;
        };
    }
    namespace Behavior{
        /*class Reader;
        template<> struct task_traits<Reader> {
            using cable_type = SOS::MemoryView::TaskCable<std::array<double,0>::iterator,2>;
        };*/
        class Reader : public SOS::Behavior::SimpleLoop<SOS::Behavior::SubController> {//, protected SOS::Behavior::Task {
            public:
            //from SimpleLoop
            using bus_type = SOS::MemoryView::ReaderBus;
            //from Task
            //using cable_type = typename SOS::Behavior::task_traits<Reader>::cable_type;
            Reader(bus_type& outside) : SOS::Behavior::SimpleLoop<SOS::Behavior::SubController>(outside.signal) {};
            void event_loop(){};
        };
        class WriteTask {
            protected:
            void write(const char* character){
                *(pos++)=*character;
            }
            private:
            std::array<char,10000> memorycontroller = std::array<char,10000>{"_"};
            std::array<char,10000>::iterator pos = memorycontroller.begin();
        };
        class WritePriority : private SOS::Behavior::RemoteSimple<Reader>, private WriteTask {
            public:
            WritePriority(
                typename SOS::Behavior::RemoteSimple<Reader>::bus_type& myBus,
                typename SOS::Behavior::RemoteSimple<Reader>::subcontroller_type::bus_type& passThru
                ) : RemoteSimple<Reader>(myBus.signal, passThru) {};
            virtual ~WritePriority(){};
            void event_loop(){
                if (!SOS::MemoryView::get<SOS::MemoryView::BusNotifier::signal_type::signal::notify>(_intrinsic).test_and_set()){
                    write("*");
                }
            };
        };
    }
}