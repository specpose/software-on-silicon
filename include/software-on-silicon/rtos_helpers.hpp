namespace SOS
{
    namespace Behavior
    {
        class Stoppable {//FPGA are not stoppable
            public:
            Stoppable() {
                request_stop();
            }
            virtual ~Stoppable(){stop();};
            virtual void event_loop()=0;
            protected:
            template<typename C> static std::thread start(C* startme){//ALWAYS requires that startme is derived from this LoopImpl
                startme->Stoppable::stop_token.getUpdatedRef().test_and_set();
                return std::move(std::thread{std::mem_fn(&C::event_loop),startme});
            }
            bool is_running() { return stop_token.getUpdatedRef().test_and_set(); }
            void finished() { stop_token.getAcknowledgeRef().clear(); }
            void request_stop() { stop_token.getUpdatedRef().clear(); }//private
            bool is_finished() { return !stop_token.getAcknowledgeRef().test_and_set(); }
            private:
            SOS::MemoryView::HandShake stop_token;
            public:
            bool stop(){//dont need thread in here
                request_stop();
                while(stop_token.getAcknowledgeRef().test_and_set()){
                    std::this_thread::yield();//caller thread, not LoopImpl
                }
                finished();
                return true;
            }
        };
        //BootstrapDummies: Can start and stop themselves via the Stoppable interface and a mix-in _thread in the impl
        template<typename... Others> class BootstrapDummySimpleController : public Stoppable, protected SimpleSubController {
            public:
            BootstrapDummySimpleController(typename bus_type::signal_type& signal, Others&... args) : Stoppable(), SimpleSubController(signal) {}
        };
        template<typename... Others> class BootstrapDummyEventController : public Stoppable, protected EventSubController {
            public:
            BootstrapDummyEventController(typename bus_type::signal_type& signal, Others&... args) : Stoppable(), EventSubController(signal) {}
        };
        //BootstrapController: Can start and stop their child and themselves via the Stoppable interface and a mix-in _thread in the impl
        template<typename S> class BootstrapAsyncController : public Controller<S>, public Stoppable {
            public:
            BootstrapAsyncController() :
            Controller<S>(),
            Stoppable(),
            _child(S{_foreign}) {}
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            public:
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
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            public:
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
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            public:
            S _child;
        };
    }
}