#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        template<size_t N> class Signals : public std::array<std::atomic_flag,N> {
        };
        class SignalsImpl : public SOS::MemoryView::Signals<1> {
            public:
            enum {
                blink
            };
        };
    }
    namespace Behavior {
        template<typename T> class EventLoop {
            public:
            using SignalType = T;
            EventLoop(SignalType& signalbus) : _intrinsic(signalbus) {}
            virtual ~EventLoop() {}
            virtual void event_loop()=0;
            protected:
            SignalType& _intrinsic;
        };

        template<typename T> class EventLoopImpl : public EventLoop<T> {
            public:
            EventLoopImpl(EventLoop<T>::SignalType& signalbus) : EventLoop<T>(signalbus) {
                auto thread = std::thread{std::mem_fn(&EventLoopImpl::event_loop),this};
                _thread.swap(thread);
            }
            ~EventLoopImpl(){
                _thread.join();
            }
            virtual void event_loop()=0;
            private:
            std::thread _thread = std::thread{};
        };

        class SubControllerImpl : public EventLoop<SOS::MemoryView::Signals<0>> {
            public:
            void event_loop(){}
        };

        template<typename SubControllerType> class Controller : public EventLoop<SOS::MemoryView::Signals<0>> {
            public:
            Controller(SignalType& signalbus) : EventLoop<SOS::MemoryView::Signals<0>>(signalbus), _wires(SubControllerType::SignalType()), _child(SubControllerType(_wires)) {
                auto thread = std::thread{std::mem_fn(&Controller::event_loop),this};
                _thread.swap(thread);
            }
            ~Controller(){
                _thread.join();
            }
            virtual void event_loop()=0;
            private:
            SubControllerType& _child;
            SubControllerType::SignalType& _wires;
            std::thread _thread = std::thread{};
        };
    }
}