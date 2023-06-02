#include <tuple>
#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        template<size_t N> class Signals : public std::array<std::atomic_flag,N> {
        };
        template<typename... T> struct TypedWires : public std::tuple<T...> {
            //GCC
            using std::tuple<T...>::tuple;
        };
    }
    namespace Behavior {
        template<size_t N> class EventLoop : public std::jthread {
            public:
            //template<typename... Q>using WireType = SOS::MemoryView::TypedWires<Q...>;
            EventLoop(SOS::MemoryView::Signals<N>& databus) : std::jthread{}, _intrinsic(databus) {
                start(*this);
            }
            protected:
            virtual void eventloop() =0;
            virtual void operator()() =0;
            SOS::MemoryView::Signals<N>& _intrinsic;
            private:
            template<typename S> void start(S& name) {
                auto thread = std::jthread{std::mem_fn(&S::eventloop),this};
                this->swap(thread);
            }
        };
    }
}