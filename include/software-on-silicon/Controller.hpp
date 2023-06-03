#include "software-on-silicon/EventLoop.hpp"

namespace SOS {
    namespace Behavior {        

        //Do not inherit from EventLoopImpl
        //thread start same as construction order: members and members that depend on members, then _thread
        template<typename T, typename S> class Controller : public EventLoop<T> {
            public:
            using SubControllerType = S;
            using SubControllerSignalType = typename SubControllerType::SignalType;
            Controller(EventLoop<T>::SignalType& signalbus) : EventLoop<T>(signalbus), _child(SubControllerType(_wires)) {
                auto thread = std::thread{std::mem_fn(&Controller::event_loop),this};
                _thread.swap(thread);
            }
            ~Controller(){
                _thread.join();
            }
            virtual void event_loop()=0;
            private:
            SubControllerType _child;
            SubControllerType::SignalType _wires = SubControllerSignalType{};
            std::thread _thread = std::thread{};
        };
    }
}
