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
                std::cout<<typeid(*this).name()<<"SN"<<std::endl;
            }
            protected:
            virtual void request_stop() final { Loop::request_stop(); }
            //SerialNotifier: event_loop in interface
            //bool is_running() { return Loop::is_running(); }
            //void finished() { Loop::finished(); }
        };
        //BootstrapDummies: Can start and stop themselves via the Stoppable interface and a mix-in _thread in the impl
        template<typename... Others> class StoppableDummyAsyncController : public Stoppable, protected SubController {
            public:
            StoppableDummyAsyncController() : Stoppable(), SubController() {}
        };
        template<typename... Others> class StoppableDummySimpleController : public Stoppable, protected SimpleSubController {
            public:
            StoppableDummySimpleController(typename bus_type::signal_type& signal, Others&... args) : Stoppable(), SimpleSubController(signal) {}
        };
        template<typename... Others> class StoppableDummyEventController : public Stoppable, protected EventSubController {
            public:
            StoppableDummyEventController(typename bus_type::signal_type& signal, Others&... args) : Stoppable(), EventSubController(signal) {}
        };
        //BootstrapController: Can start and stop their child and themselves via the Stoppable interface and a mix-in _thread in the impl
        template<typename S> class BootstrapAsyncController : public Controller<S>, public Stoppable {
            public:
            BootstrapAsyncController() :
            Controller<S>(),
            Stoppable(),
            _child(new S{_foreign}) {}
            ~BootstrapAsyncController(){
                if (_child) {
                    //SFA::util::runtime_error(SFA::util::error_code::ChildHasToBeDeletedBeforeDestroyThread, __FILE__, __func__, typeid(*this).name());
                    delete _child;
                    _child = nullptr;
                }
            }
            void stop_descendants() {
                if (_child)
                    delete _child;
                else
                    SFA::util::runtime_error(SFA::util::error_code::ChildHasAlreadyBeenDeleted, __FILE__, __func__, typeid(*this).name());
                _child = nullptr;
            }
            void start_descendants() {
                if (!_child)
                    _child = new S{_foreign};
                else
                    SFA::util::runtime_error(SFA::util::error_code::ChildIsAlreadyRunningOrHasNotBeenDeleted, __FILE__, __func__, typeid(*this).name());
            }
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S* _child = nullptr;
        };
        template<typename S> class BootstrapSimpleController : public Controller<S>, public Stoppable, protected SimpleSubController {
            public:
            BootstrapSimpleController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Stoppable(),
            SimpleSubController(signal),
            _child(new S{_foreign})
            {}
            ~BootstrapSimpleController(){
                if (_child) {
                    //SFA::util::runtime_error(SFA::util::error_code::ChildHasToBeDeletedBeforeDestroyThread, __FILE__, __func__, typeid(*this).name());
                    delete _child;
                    _child = nullptr;
                }
            }
            void stop_descendants() {
                if (_child) {
                    delete _child;
                    _child = nullptr;
                } else {
                    SFA::util::runtime_error(SFA::util::error_code::ChildHasAlreadyBeenDeleted, __FILE__, __func__, typeid(*this).name());
                }
            }
            void start_descendants() {
                if (!_child)
                    _child = new S{_foreign};
                else
                    SFA::util::runtime_error(SFA::util::error_code::ChildIsAlreadyRunningOrHasNotBeenDeleted, __FILE__, __func__, typeid(*this).name());
            }
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S* _child = nullptr;
        };
        template<typename S> class BootstrapEventController : public Controller<S>, public Stoppable, protected EventSubController {
            public:
            BootstrapEventController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Stoppable(),
            EventSubController(signal),
            _child(new S{_foreign})
            {}
            ~BootstrapEventController(){
                if (_child) {
                    //SFA::util::runtime_error(SFA::util::error_code::ChildHasToBeDeletedBeforeDestroyThread, __FILE__, __func__, typeid(*this).name());
                    delete _child;
                    _child = nullptr;
                }
            }
            void stop_descendants() {
                if (_child)
                    delete _child;
                else
                    SFA::util::runtime_error(SFA::util::error_code::ChildHasAlreadyBeenDeleted, __FILE__, __func__, typeid(*this).name());
                _child = nullptr;
            }
            void start_descendants() {
                if (!_child)
                    _child = new S{_foreign};
                else
                    SFA::util::runtime_error(SFA::util::error_code::ChildIsAlreadyRunningOrHasNotBeenDeleted, __FILE__, __func__, typeid(*this).name());
            }
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S* _child = nullptr;
        };
    }
}
