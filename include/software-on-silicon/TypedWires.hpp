#include <tuple>
#include <atomic>

namespace SOS{
    namespace MemoryView {
        template<typename T> class TypedWires : public std::tuple<std::atomic<T>> {
            
        };
    }
    namespace Behavior {
        template<typename T> class EventHandler {
            public:
            EventHandler(SOS::MemoryView::TypedWires<T>& databus) : _wires(databus) {}
            virtual void operator()()=0;
            protected:
            SOS::MemoryView::TypedWires<T>& _wires;
        };
    }
}