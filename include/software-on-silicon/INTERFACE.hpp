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
        template<typename T> class Contiguous : public std::vector<T> {
            public:
            using value_type = T;
            Contiguous() = delete;
            Contiguous(const std::size_t size) : std::vector<T>(size) {
                if (this->size()!=size)
                    throw SFA::util::logic_error("Contiguous initialized incorrectly",__FILE__,__func__);
                std::fill(this->begin(),this->end(),0.0);
            }
            ~Contiguous(){
                //error: double free detected
                /*if (_storage!=nullptr) {
                    delete _storage;
                    _storage = nullptr;
                }*/
            }
            Contiguous& operator=(const std::vector<T>& other){
                if (other.size()!=this->size())
                    throw SFA::util::logic_error("operator=() used incorrectly",__FILE__,__func__);
                for (std::size_t channel=0;channel<std::vector<T>::size();channel++)
                    (*this)[channel]=other[channel];
                return *this;
            }
        };
    }
    namespace Behavior {
        //Loops always have at least one signal for termination acknowledge if used for single run
        class Loop {
            public:
            Loop() {
                request_stop();
            }
            virtual ~Loop(){stop();};
            virtual void event_loop()=0;
            protected:
            template<typename C> static std::thread start(C* startme){//ALWAYS requires that startme is derived from this LoopImpl
                startme->Loop::stop_token.getUpdatedRef().test_and_set();
                return std::move(std::thread{std::mem_fn(&C::event_loop),startme});
            }
            bool is_running() { return stop_token.getUpdatedRef().test_and_set(); }
            void finished() { stop_token.getAcknowledgeRef().clear(); }
            void request_stop() { stop_token.getUpdatedRef().clear(); }//private
            bool is_finished() { return !stop_token.getAcknowledgeRef().test_and_set(); }
            private:
            SOS::MemoryView::HandShake stop_token;
            bool stop(){//dont need thread in here
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
        template<typename... Others> class DummySimpleController : public Loop, protected SubController {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            DummySimpleController(typename bus_type::signal_type& signal, Others&... args) : Loop(), SubController(), _intrinsic(signal) {}
            protected:
            bus_type::signal_type& _intrinsic;
        };
        template<typename... Others> class DummyEventController : public Loop, protected SubController {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            DummyEventController(typename bus_type::signal_type& signal, Others&... args) : Loop(), SubController(), _intrinsic(signal) {}
            protected:
            bus_type::signal_type& _intrinsic;
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
        template<typename S> class SimpleController : public Controller<S>, public Loop, protected SubController {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            SimpleController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Loop(),
            SubController(),
            _intrinsic(signal),
            _child(S{_foreign})
            {}
            protected:
            bus_type::signal_type& _intrinsic;
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S _child;
        };
        template<typename S> class EventController : public Controller<S>, public Loop, protected SubController {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            EventController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Loop(),
            SubController(),
            _intrinsic(signal),
            _child(S{_foreign})
            {}
            protected:
            bus_type::signal_type& _intrinsic;
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S _child;
        };
    }
}