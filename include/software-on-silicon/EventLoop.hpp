#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        template<size_t N> class Signals : public std::array<std::atomic_flag,N> {
        };
    }
    namespace Behavior {
        template<typename SubControllerType> class EventLoop {
            public:
            using SignalType = SOS::MemoryView::Signals<0>;
            EventLoop(SignalType& signalbus) : _intrinsic(signalbus), _wires(SubControllerType::SignalType()), _child(SubControllerType(_wires)) {
                auto thread = std::thread{std::mem_fn(&EventLoop::event_loop),this};
                _thread.swap(thread);
            }
            ~EventLoop(){
                _thread.join();
            }
            virtual void event_loop()=0;
            private:
            SignalType& _intrinsic;
            std::thread _thread = std::thread{};
            SubControllerType& _child;
            SubControllerType::SignalType& _wires;
        };
    }
}