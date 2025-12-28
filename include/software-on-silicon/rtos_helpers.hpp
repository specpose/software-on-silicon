namespace SOS
{
    namespace Behavior
    {
        class Stoppable : public Loop {//FPGA are not stoppable
            public:
            Stoppable() : Loop() {}
            ~Stoppable(){
                //if (!is_finished())
                //    throw SFA::util::logic_error("stop() has not been called on Stoppable.", __FILE__, __func__);
            }
            public:
            virtual void destroy(std::thread& destroyme) final {
                stop();
                destroyme.join();
            }
            virtual void request_stop() final { Loop::request_stop(); }
            virtual bool stop() final { bool result = Loop::stop(); std::cout<<typeid(*this).name()<<"SN"<<std::endl; return result; }
            virtual void restart() = 0;//Do not change memory of thread object in here, assume persistent
        };
        //BootstrapDummies: Can start and stop themselves via the Stoppable interface and a mix-in _thread in the impl
        template<typename... Others> class BootstrapDummyAsyncController : public Stoppable, protected SubController {
            public:
            BootstrapDummyAsyncController() : Stoppable(), SubController() {}
            bool stop_children() { return true; }
            void restart_children() {}
        };
        template<typename... Others> class BootstrapDummySimpleController : public Stoppable, protected SimpleSubController {
            public:
            BootstrapDummySimpleController(typename bus_type::signal_type& signal, Others&... args) : Stoppable(), SimpleSubController(signal) {}
            bool stop_children() { return true; }
            void restart_children() {}
        };
        template<typename... Others> class BootstrapDummyEventController : public Stoppable, protected EventSubController {
            public:
            BootstrapDummyEventController(typename bus_type::signal_type& signal, Others&... args) : Stoppable(), EventSubController(signal) {}
            bool stop_children() { return true; }
            void restart_children() {}
        };
        //BootstrapController: Can start and stop their child and themselves via the Stoppable interface and a mix-in _thread in the impl
        template<typename S> class BootstrapAsyncController : public Controller<S>, public Stoppable {
            public:
            BootstrapAsyncController() :
            Controller<S>(),
            Stoppable(),
            _child(S{_foreign}) {}
            bool stop_children() { _child.stop_children(); _child.stop(); return true; }
            void restart_children() { _child.restart_children(); _child.restart(); }
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S _child;
        };
        template<typename S> class BootstrapSimpleController : public Controller<S>, public Stoppable, protected SimpleSubController {
            public:
            BootstrapSimpleController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Stoppable(),
            SimpleSubController(signal),
            _child(S{_foreign})
            {}
            bool stop_children() { _child.stop_children(); _child.stop(); return true; }
            void restart_children() { _child.restart_children(); _child.restart(); }
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S _child;
        };
        template<typename S> class BootstrapEventController : public Controller<S>, public Stoppable, protected EventSubController {
            public:
            BootstrapEventController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Stoppable(),
            EventSubController(signal),
            _child(S{_foreign})
            {}
            bool stop_children() { _child.stop_children(); _child.stop(); return true; }
            void restart_children() { _child.restart_children(); _child.restart(); }
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S _child;
        };
    }
}
