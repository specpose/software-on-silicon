#include <array>
#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        class Notify : private std::array<std::atomic_flag,1> {
            public:
            Notify() : std::array<std::atomic_flag,1>{true} {}
            auto& getNotifyRef(){return std::get<0>(*this);}
        };
        //1+1=0
        class HandShake : private std::array<std::atomic_flag,2> {
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
        //Loops always have at least one signal for termination acknowledge if used for single run
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
            protected:
            SOS::MemoryView::HandShake stop_token;
            template<typename C> static std::thread start(C* startme){//ALWAYS requires that startme is derived from this LoopImpl
                startme->Loop::stop_token.getUpdatedRef().test_and_set();
                return std::move(std::thread{std::mem_fn(&C::event_loop),startme});
            }
        };
        struct subcontroller_tag{};
        template<typename LoopSignalType> class _DummyController : public subcontroller_tag {
            public:
            _DummyController(LoopSignalType& signal) : _intrinsic(signal) {}
            protected:
            LoopSignalType& _intrinsic;
        };
        template<typename T, typename S = typename std::enable_if<
                std::is_base_of< SOS::Behavior::subcontroller_tag,T >::value,T
                >::type > class _Async {
            public:
            using subcontroller_type = S;
            _Async() {}
        };
        //Use Implementations (SimpleController<EventController>), not directly (_Controller<SubController>) in cascading definitions 
        //A Blink doesnt need a Bus
        template<typename LoopSignalType, typename T> class _Controller : public SOS::Behavior::_Async<T>, public subcontroller_tag {
            public:
            _Controller(LoopSignalType& signal) : _Async<T>(), _intrinsic(signal) {}
            protected:
            LoopSignalType& _intrinsic;
        };
        template<typename S> class Async : private _Async<S> {
            public:
            Async() :
            _Async<S>(),
            _child(typename _Async<S>::subcontroller_type{_foreign}) {}
            protected:
            typename _Async<S>::subcontroller_type::bus_type _foreign = typename _Async<S>::subcontroller_type::bus_type{};
            typename _Async<S>::subcontroller_type _child;
        };
        template<typename... Others> class DummySimpleController : protected _DummyController<SOS::MemoryView::Notify> {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            DummySimpleController(typename bus_type::signal_type& signal, Others&... args) : _DummyController<SOS::MemoryView::Notify>(signal) {}
        };
        template<typename... Others> class DummyEventController : protected _DummyController<SOS::MemoryView::HandShake> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            DummyEventController(typename bus_type::signal_type& signal, Others&... args) : _DummyController<SOS::MemoryView::HandShake>(signal) {}
        };
        //bus_type is ALWAYS locally constructed in upstream Controller<SimpleController> or MUST be undefined
        template<typename S, typename... Others> class SimpleController : private _Controller<SOS::MemoryView::Notify, S> {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            SimpleController(typename bus_type::signal_type& signal, Others&... args) :
            _Controller<SOS::MemoryView::Notify, S>(signal),
            _child(typename _Controller<SOS::MemoryView::Notify, S>::subcontroller_type{_foreign, args...})
            {}
            protected:
            typename _Controller<SOS::MemoryView::Notify, S>::subcontroller_type::bus_type _foreign = typename _Controller<SOS::MemoryView::Notify, S>::subcontroller_type::bus_type{};
            typename _Controller<SOS::MemoryView::Notify, S>::subcontroller_type _child;
        };
        template<typename S, typename... Others> class EventController : protected _Controller<SOS::MemoryView::HandShake, S> {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            EventController(typename bus_type::signal_type& signal, Others&... args) :
            _Controller<SOS::MemoryView::HandShake, S>(signal),
            _child(typename _Controller<SOS::MemoryView::HandShake, S>::subcontroller_type{_foreign, args...})
            {}
            protected:
            typename _Controller<SOS::MemoryView::HandShake, S>::subcontroller_type::bus_type _foreign = typename _Controller<SOS::MemoryView::HandShake, S>::subcontroller_type::bus_type{};
            typename _Controller<SOS::MemoryView::HandShake, S>::subcontroller_type _child;
        };
    }
}