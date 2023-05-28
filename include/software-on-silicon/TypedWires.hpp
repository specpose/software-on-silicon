#include <tuple>
#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        template<typename... T> struct TypedWires : public std::tuple<T...> {
            using std::tuple<T...>::tuple;
        };
    }
    namespace Behavior {
        template<typename... T> class EventLoop : public std::jthread {
            public:
            template<typename... Q>using WireType = SOS::MemoryView::TypedWires<Q...>;
            EventLoop(WireType<T...>& databus) : std::jthread{}, _intrinsic(databus) {
                auto thread = std::jthread{std::mem_fn(&EventLoop<T...>::eventloop),this};
                this->swap(thread);
            }
            //~EventLoop() {this->join();}
            protected:
            virtual void eventloop() =0;
            virtual void operator()() =0;
            WireType<T...>& _intrinsic;
        };
    }
}