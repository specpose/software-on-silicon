#include <array>
#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        class Notify : public std::array<std::atomic_flag,1> {
            public:
            Notify() : std::array<std::atomic_flag,1>{true} {}
            auto& getNotifyRef(){return std::get<0>(*this);}
        };
        //1+1=0
        class HandShake : public std::array<std::atomic_flag,2> {
            public:
            HandShake() : std::array<std::atomic_flag,2>{true,true} {}
            auto& getUpdatedRef(){return std::get<0>(*this);}
            auto& getAcknowledgeRef(){return std::get<1>(*this);}
        };
        template<typename T, size_t N> struct ConstCable : public std::array<const T,N>{
            using wire_names = enum class empty : unsigned char{} ;
            using cable_arithmetic = T;
        };
        template<typename T, size_t N> struct TaskCable : public std::array<std::atomic<T>,N>{
            using wire_names = enum class empty : unsigned char{} ;
            using cable_arithmetic = T;
        };
        //inheritable
        template<typename Category, typename Signal, typename Cables, typename ConstCables> struct bus {
            using bus_category = Category;
            using signal_type = Signal;
            using cables_type = Cables;
            using const_cables_type = ConstCables;
        };
        //specialisable
        template<typename bus> struct bus_traits {
            using bus_category = typename bus::bus_category;
            using signal_type = typename bus::signal_type;
            using cables_type = typename bus::cables_type;
            using const_cables_type = typename bus::const_cables_type;
        };
        struct empty_bus_tag{};
        struct Bus : bus<
            empty_bus_tag,
            std::array<std::atomic_flag,0>,
            std::tuple< >,
            std::tuple< >
        >{};
        struct bus_notifier_tag{};
        struct BusNotifier : bus <
            bus_notifier_tag,
            SOS::MemoryView::Notify,
            bus_traits<Bus>::cables_type,
            bus_traits<Bus>::const_cables_type
        >{
            signal_type signal;
        };
        struct bus_shaker_tag{};
        struct BusShaker : bus <
            bus_shaker_tag,
            SOS::MemoryView::HandShake,
            bus_traits<Bus>::cables_type,
            bus_traits<Bus>::const_cables_type
        >{
            signal_type signal;
        };
    }
    namespace Behavior {
        class Loop {
            public:
            Loop() {
                stop_token.getUpdatedRef().clear();
            }
            virtual ~Loop(){};//for thread
            virtual void event_loop()=0;
            bool stop(){//dont need thread in here
                stop_token.getUpdatedRef().clear();
                while(stop_token.getAcknowledgeRef().test_and_set()){
                    std::this_thread::yield();//caller thread, not LoopImpl
                }
                stop_token.getAcknowledgeRef().clear();
                return true;
            }
            SOS::MemoryView::HandShake stop_token;
            protected:
            template<typename C> static std::thread start(C* startme){//ALWAYS requires that startme is derived from this LoopImpl
                startme->stop_token.getUpdatedRef().test_and_set();
                return std::move(std::thread{std::mem_fn(&C::event_loop),startme});
            }
        };
        class DummyController {};
        //Use Implementations (SimpleController<EventController>), not directly (Controller<SubController>) in cascading definitions 
        //A Blink doesnt need a Bus
        template<typename LoopSignalType, typename S=DummyController> class Controller {
            public:
            using subcontroller_type = S;
            Controller(LoopSignalType& signal) : _intrinsic(signal) {}
            protected:
            LoopSignalType& _intrinsic;
        };
        //bus_type is ALWAYS locally constructed in upstream Controller<SimpleController> or MUST be undefined
        template<typename S, typename... Others> class SimpleController : public Controller<SOS::MemoryView::Notify, S> {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            using subcontroller_type = typename Controller<SOS::MemoryView::Notify, S>::subcontroller_type;
            SimpleController(typename bus_type::signal_type& signal, Others&... args) :
            Controller<SOS::MemoryView::Notify, S>(signal),
            _child(subcontroller_type{_foreign, args...})
            {}
            protected:
            typename subcontroller_type::bus_type _foreign = typename subcontroller_type::bus_type{};
            S _child;
        };
        template<> class SimpleController<DummyController> : public Controller<SOS::MemoryView::Notify, DummyController> {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            using subcontroller_type = DummyController;
            SimpleController(typename bus_type::signal_type& signal) :
            Controller<SOS::MemoryView::Notify, DummyController>(signal)
            {}
        };
        template<typename S, typename... Others> class EventController : public Controller<SOS::MemoryView::HandShake, S> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            using subcontroller_type = typename Controller<SOS::MemoryView::HandShake, S>::subcontroller_type;
            EventController(typename bus_type::signal_type& signal, Others&... args) :
            Controller<SOS::MemoryView::HandShake, S>(signal),
            _child(subcontroller_type{_foreign, args...})
            {}
            protected:
            typename subcontroller_type::bus_type _foreign = typename subcontroller_type::bus_type{};
            S _child;
        };
        template<> class EventController<DummyController> : public Controller<SOS::MemoryView::HandShake, DummyController> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            using subcontroller_type = DummyController;
            EventController(typename bus_type::signal_type& signal) :
            Controller<SOS::MemoryView::HandShake, DummyController>(signal)
            {}
        };
    }
}