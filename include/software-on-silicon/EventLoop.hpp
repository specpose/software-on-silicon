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
            using bus_type = SOS::MemoryView::Bus;
            virtual ~Loop()=0;
            virtual void event_loop()=0;
            protected:
            template<typename C> static std::thread start(C* startme){
                return std::move(std::thread{std::mem_fn(&C::event_loop),startme});
            }
        };
        Loop::~Loop(){}
        template<typename S> class RunLoop : public Loop {
            public:
            using bus_type = SOS::MemoryView::Bus;
            using subcontroller_type = S;
            RunLoop() : _child(subcontroller_type{_foreign}) {}
            virtual ~RunLoop() override {};
            protected:
            typename subcontroller_type::bus_type _foreign = typename subcontroller_type::bus_type{};
            private:
            S _child;
        };
        class SubController {
            public:
            using bus_type = SOS::MemoryView::Bus;
            SubController(bus_type& bus){}
        };
        template<typename S> class SimpleLoop : public Loop {
            public:
            using bus_type = SOS::MemoryView::BusNotifier;
            using subcontroller_type = S;
            SimpleLoop(bus_type::signal_type& ground) : _intrinsic(ground), _child(subcontroller_type{_foreign}) {}
            virtual ~SimpleLoop() override {};
            protected:
            bus_type::signal_type& _intrinsic;
            typename subcontroller_type::bus_type _foreign = typename subcontroller_type::bus_type{};
            private:
            S _child;
        };
        template<typename S> class EventLoop : public Loop {
            public:
            using bus_type = SOS::MemoryView::BusShaker;
            using subcontroller_type = S;
            EventLoop(bus_type::signal_type& ground) : _intrinsic(ground), _child(subcontroller_type{_foreign}) {}
            virtual ~EventLoop() override {};
            protected:
            bus_type::signal_type& _intrinsic;
            typename subcontroller_type::bus_type _foreign = typename subcontroller_type::bus_type{};
            private:
            S _child;
        };
    }
}