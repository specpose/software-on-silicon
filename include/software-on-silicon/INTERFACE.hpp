#include <array>
#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        class Notify {
            public:
            Notify() { notify.test_and_set(); }
            auto& getNotifyRef() { return notify; }
            protected:
            std::atomic_flag notify {};
        };
        class Pair : private Notify, public std::array<std::atomic_flag,1> {
            public:
            Pair() : Notify(), std::array<std::atomic_flag,1>{} {}
            auto& getFirstRef() { return notify; }
            auto& getSecondRef() { return std::get<0>(*this); }
        };
        //1+1=0
        class HandShake {
            public:
            HandShake() { updated.test_and_set(); acknowledge.test_and_set(); }
            auto& getUpdatedRef() { return updated; }
            auto& getAcknowledgeRef() { return acknowledge; }
            protected:
            std::atomic_flag updated {};
            std::atomic_flag acknowledge {};
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
        class Stoppable;
        //Loops always have at least one signal for termination acknowledge if used for single run
        class Loop {
        	friend Stoppable;
            public:
            Loop() {
                //if (!is_finished())
                //    throw SFA::util::logic_error("Loop has not come out of while loop or request_stop() has not been called.", __FILE__, __func__);
            }
            virtual ~Loop(){stop();};
            virtual void event_loop()=0;
            protected:
            template<typename C> static std::thread start(C* startme){//ALWAYS requires that startme is derived from this LoopImpl
                startme->Loop::stop_token.getUpdatedRef().test_and_set();
                return std::move(std::thread{std::mem_fn(&C::_stoppable_loop),startme});
            }
            virtual void destroy(std::thread& destroyme){
                request_stop();
                destroyme.join();
            }
            private:
            bool is_running() { return stop_token.getUpdatedRef().test_and_set(); }
            void finished() { stop_token.getAcknowledgeRef().clear(); }
            bool is_finished() { return !stop_token.getAcknowledgeRef().test_and_set(); }
            void _stoppable_loop(){
                while (is_running()) {
                    event_loop();
                }
                finished();
            }
            SOS::MemoryView::HandShake stop_token;
            virtual void request_stop() { stop_token.getUpdatedRef().clear(); }
            virtual bool stop() {//dont need thread in here
                request_stop();
                while(stop_token.getAcknowledgeRef().test_and_set()){
                    std::this_thread::yield();//caller thread, not LoopImpl
                }
                finished();
                return true;
            }
        };
        //Use Implementations (SimpleController<EventController>), not directly (Controller<SubController>) in cascading definitions 
        //A Blink doesnt need a Bus
        class SubController {
            public:
            constexpr SubController() {}
        };
        class SimpleSubController : public SubController {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            constexpr SimpleSubController(typename bus_type::signal_type& signal) : SubController(), _intrinsic(signal) {}
            protected:
            bus_type::signal_type& _intrinsic;
        };
        class EventSubController : public SubController {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            constexpr EventSubController(typename bus_type::signal_type& signal) : SubController(), _intrinsic(signal) {}
            protected:
            bus_type::signal_type& _intrinsic;
        };
        template<typename... Others> class AsyncDummy : public Loop, protected SubController {
            public:
            AsyncDummy() : Loop(), SubController() {}
        };
        template<typename... Others> class SimpleDummy : public Loop, protected SimpleSubController {
            public:
            SimpleDummy(typename bus_type::signal_type& signal, Others&... args) : Loop(), SimpleSubController(signal) {}
        };
        template<typename... Others> class EventDummy : public Loop, protected EventSubController {
            public:
            EventDummy(typename bus_type::signal_type& signal, Others&... args) : Loop(), EventSubController(signal) {}
        };
        template<typename T, typename S = typename std::enable_if<
                std::is_base_of< typename SOS::Behavior::SubController,T >::value,T
                //false,T
                >::type > class Controller {
            public:
            //using subcontroller_type = S;
            constexpr Controller() {}
        };
        //bus_type is ALWAYS locally constructed in upstream Controller<SimpleController> or MUST be undefined
        template<typename S> class AsyncController : public Controller<S>, public Loop {
            public:
            AsyncController() :
            Controller<S>(),
            Loop(),
            _child(S{_foreign}) {}
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S _child;
        };
        template<typename S> class SimpleController : public Controller<S>, public Loop, protected SimpleSubController {
            public:
            SimpleController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Loop(),
            SimpleSubController(signal),
            _child(S{_foreign})
            {}
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S _child;
        };
        template<typename S> class EventController : public Controller<S>, public Loop, protected EventSubController {
            public:
            EventController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Loop(),
            EventSubController(signal),
            _child(S{_foreign})
            {}
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S _child;
        };
    }
}
