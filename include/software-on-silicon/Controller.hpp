#pragma once
#include "software-on-silicon/EventLoop.hpp"

namespace SOS {
    namespace Behavior {        

        //thread start same as construction order: members and members that depend on members, then _thread
        template<typename S> class Controller : public RunLoop {
            public:
            using subcontroller_type = S;
            Controller() : RunLoop(), _child(S(_foreign)) {}
            virtual ~Controller(){}
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            //avoid duplicate memory!
            S _child;
        };
    }
}
