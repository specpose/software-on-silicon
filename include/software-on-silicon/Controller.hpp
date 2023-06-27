#pragma once
#include "software-on-silicon/EventLoop.hpp"

namespace SOS {
    namespace Behavior {        

        //thread start same as construction order: members and members that depend on members, then _thread
        template<typename S> class Controller : public SOS::Behavior::SimpleLoop {
            public:
            using subcontroller_type = S;
            Controller(bus_type::signal_type& signal) : SOS::Behavior::SimpleLoop(signal) {}
            virtual ~Controller(){}
        };
        template<typename S> class Local : public Controller<S> {
            public:
            Local(SOS::Behavior::SimpleLoop::bus_type::signal_type& signal) : Controller<S>(signal), _child(S(_foreign)) {}
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S _child;
        };
        template<typename S> class Remote : public Controller<S> {
            public:
            Remote(SOS::Behavior::SimpleLoop::bus_type::signal_type& signal,typename S::bus_type& remote) : Controller<S>(signal), _foreign(remote), _child(S(_foreign)) {}
            protected:
            typename S::bus_type& _foreign;
            private:
            S _child;
        };
    }
}
