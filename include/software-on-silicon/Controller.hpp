#include "software-on-silicon/EventLoop.hpp"

namespace SOS {
    namespace Behavior {        

        //thread start same as construction order: members and members that depend on members, then _thread
        template<typename T, typename S> class Controller : public EventLoop<T> {
            public:
            using SubControllerType = S;
            Controller(EventLoop<T>::SignalType& signalbus) : EventLoop<T>(signalbus), _child(SubControllerType(_wires)) {}
            virtual ~Controller(){}
            virtual void event_loop()=0;
            protected:
            SubControllerType::SignalType _wires = typename S::SignalType{};
            private:
            SubControllerType _child;
        };
    }
}
