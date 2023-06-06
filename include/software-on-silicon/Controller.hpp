#include "software-on-silicon/EventLoop.hpp"

namespace SOS {
    namespace Behavior {        

        //thread start same as construction order: members and members that depend on members, then _thread
        template<typename T, typename S> class Controller : public EventLoop<T> {
            public:
            Controller(typename EventLoop<T>::bus_type& bus) : EventLoop<T>(bus), _child(S(_foreign)) {}
            virtual ~Controller(){}
            protected:
            typename S::bus_type _foreign = make_bus(_signal,_data);
            private:
            SOS::MemoryView::HandShake _signal = SOS::MemoryView::HandShake{};
            typename S::bus_type::data_type _data = typename S::bus_type::data_type{};
            S _child;
        };
    }
}
