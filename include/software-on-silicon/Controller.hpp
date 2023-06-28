#pragma once
#include "software-on-silicon/EventLoop.hpp"

namespace SOS {
    namespace Behavior {        
        class SubController {
            public:
            using bus_type = SOS::MemoryView::Bus;
        };
        //thread start same as construction order: members and members that depend on members, then _thread
        template<typename S> class LocalRun : public RunLoop<S> {
            public:
            LocalRun() : RunLoop<S>(), _child(S(_foreign)) {}
            protected:
            typename RunLoop<S>::subcontroller_type::bus_type _foreign = typename RunLoop<S>::subcontroller_type::bus_type{};
            private:
            S _child;
        };
        /*template<typename S> class LocalSimple : public SimpleLoop<S> {
            public:
            LocalSimple(typename SOS::Behavior::SimpleLoop<S>::bus_type::signal_type& signal) : SimpleLoop<S>(signal), _child(S(_foreign)) {}
            protected:
            typename SimpleLoop<S>::subcontroller_type::bus_type _foreign = typename SimpleLoop<S>::subcontroller_type::bus_type{};
            private:
            S _child;
        };*/
        /*template<typename S> class RemoteSimple : public SimpleLoop<S> {
            public:
            RemoteSimple(typename SOS::Behavior::SimpleLoop<S>::bus_type::signal_type& signal,typename S::bus_type& remote) : SimpleLoop<S>(signal), _foreign(remote), _child(S(_foreign)) {}
            protected:
            typename SimpleLoop<S>::subcontroller_type::bus_type& _foreign;
            private:
            S _child;
        };*/
    }
}
