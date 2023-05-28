#include <tuple>
#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        template<typename T> struct TypedWires : public std::tuple<std::atomic<T>> {};
    }
    namespace Behavior {
        template<typename T> class EventHandler : public std::jthread {
            public:
            using WireType = SOS::MemoryView::TypedWires<T>;
            EventHandler(WireType& databus) : std::jthread{}, _intrinsic(databus) {
                auto thread = std::jthread{std::mem_fn(&EventHandler<T>::eventloop),this};
                this->swap(thread);
            }
            //~EventHandler() {this->join();}
            protected:
            virtual void eventloop() =0;
            virtual void operator()() =0;
            WireType& _intrinsic;
        };
    }
}