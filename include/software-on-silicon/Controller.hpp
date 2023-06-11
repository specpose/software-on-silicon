#pragma once
#include "software-on-silicon/EventLoop.hpp"

namespace SOS {
    namespace Behavior {        

        //thread start same as construction order: members and members that depend on members, then _thread
        template<typename S> class Controller : public RunLoop {
            public:
            using subcontroller_type = typename SOS::MemoryView::BusNotifier<S>;
            Controller() : RunLoop(), _child(S(_foreign)) {}
            virtual ~Controller(){}
            protected:
            typename SOS::MemoryView::BusNotifier<S> _foreign = subcontroller_type{};
            private:
            S _child;
        };
    }
}
