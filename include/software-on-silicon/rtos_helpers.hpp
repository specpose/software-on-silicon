namespace SOS
{
    namespace MemoryView
    {
        class AsyncAndHandShake : private std::array<std::atomic_flag,2> {
        public:
            AsyncAndHandShake() : std::array<std::atomic_flag,2>{} {
                std::get<0>(*this).test_and_set();
                std::get<1>(*this).test_and_set();
            }
            auto& getAuxUpdatedRef(){return std::get<0>(*this);}
            auto& getAuxAcknowledgeRef(){return std::get<1>(*this);}
        };
        class NotifierAndHandShake : private std::array<std::atomic_flag,3> {
        public:
            NotifierAndHandShake() : std::array<std::atomic_flag,3>{} {
                std::get<0>(*this).test_and_set();
                std::get<1>(*this).test_and_set();
                std::get<2>(*this).test_and_set();
            }
            auto& getNotifyRef(){return std::get<0>(*this);}
            auto& getAuxUpdatedRef(){return std::get<1>(*this);}
            auto& getAuxAcknowledgeRef(){return std::get<2>(*this);}
        };
        class DoubleHandShake : private std::array<std::atomic_flag,4> {
        public:
            DoubleHandShake() : std::array<std::atomic_flag,4>{} {
                std::get<0>(*this).test_and_set();
                std::get<1>(*this).test_and_set();
                std::get<2>(*this).test_and_set();
                std::get<3>(*this).test_and_set();
            }
            auto& getUpdatedRef(){return std::get<0>(*this);}
            auto& getAcknowledgeRef(){return std::get<1>(*this);}
            auto& getAuxUpdatedRef(){return std::get<2>(*this);}
            auto& getAuxAcknowledgeRef(){return std::get<3>(*this);}
        };
        struct bus_async_and_handshake_tag{};
        struct BusAsyncAndShaker : bus <
        bus_async_and_handshake_tag,
        SOS::MemoryView::AsyncAndHandShake,
        bus_traits<Bus>::cables_type,
        bus_traits<Bus>::const_cables_type
        >{
            signal_type signal;
        };
        struct bus_notifier_and_handshake_tag{};
        struct BusNotifierAndShaker : bus <
        bus_notifier_and_handshake_tag,
        SOS::MemoryView::NotifierAndHandShake,
        bus_traits<Bus>::cables_type,
        bus_traits<Bus>::const_cables_type
        >{
            signal_type signal;
        };
        struct bus_double_handshake_tag{};
        struct BusDoubleShaker : bus <
        bus_double_handshake_tag,
        SOS::MemoryView::DoubleHandShake,
        bus_traits<Bus>::cables_type,
        bus_traits<Bus>::const_cables_type
        >{
            signal_type signal;
        };
    }
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
            //virtual void request_stop() final { Loop::request_stop(); }
            //SerialNotifier: event_loop in interface
            //bool is_running() { return Loop::is_running(); }
            //void finished() { Loop::finished(); }
        };
        class StoppableAsyncSubController : public SubController {
        public:
            using bus_type = SOS::MemoryView::BusAsyncAndShaker;
            StoppableAsyncSubController(typename bus_type::signal_type& signal) : SubController(), _intrinsic(signal) {}
        protected:
            bus_type::signal_type& _intrinsic;
        };
        class StoppableSimpleSubController : public SubController {
        public:
            using bus_type = SOS::MemoryView::BusNotifierAndShaker;
            StoppableSimpleSubController(typename bus_type::signal_type& signal) : SubController(), _intrinsic(signal) {}
        protected:
            bus_type::signal_type& _intrinsic;
        };
        class StoppableEventSubController : public SubController {
        public:
            using bus_type = SOS::MemoryView::BusDoubleShaker;
            StoppableEventSubController(typename bus_type::signal_type& signal) : SubController(), _intrinsic(signal) {}
        protected:
            bus_type::signal_type& _intrinsic;
        };
        //BootstrapDummies: Can start and stop themselves via the Stoppable interface and a mix-in _thread in the impl
        /*template<typename... Others> class StoppableDummyAsyncController : public Stoppable, protected SubController {
            public:
            StoppableDummyAsyncController() : Stoppable(), SubController() {}
        };
        template<typename... Others> class StoppableDummySimpleController : public Stoppable, protected StoppableSimpleSubController {
            public:
            StoppableDummySimpleController(typename bus_type::signal_type& signal, Others&... args) : Stoppable(), StoppableSimpleSubController(signal) {}
        };
        template<typename... Others> class StoppableDummyEventController : public Stoppable, protected StoppableEventSubController {
            public:
            StoppableDummyEventController(typename bus_type::signal_type& signal, Others&... args) : Stoppable(), StoppableEventSubController(signal) {}
        };*/
        //BootstrapController: Can start and stop their child and themselves via the Stoppable interface and a mix-in _thread in the impl
        template<typename S> class BootstrapAsyncController : public Controller<S>, public Stoppable, protected StoppableAsyncSubController {
            public:
            BootstrapAsyncController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Stoppable(),
            StoppableAsyncSubController(signal),
            _child(new S{_foreign}) {}
            ~BootstrapAsyncController(){
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
            bool descendants_stopped () { return !_child; }
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S* _child = nullptr;
        };
        template<typename S> class BootstrapSimpleController : public Controller<S>, public Stoppable, protected StoppableSimpleSubController {
            public:
            BootstrapSimpleController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Stoppable(),
            StoppableSimpleSubController(signal),
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
            bool descendants_stopped () { return !_child; }
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S* _child = nullptr;
        };
        template<typename S> class BootstrapEventController : public Controller<S>, public Stoppable, protected StoppableEventSubController {
            public:
            BootstrapEventController(typename bus_type::signal_type& signal) :
            Controller<S>(),
            Stoppable(),
            StoppableEventSubController(signal),
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
                if (_child) {
                    delete _child;
                    _child = nullptr;
                } else {
                    SFA::util::runtime_error(SFA::util::error_code::ChildHasAlreadyBeenDeleted, __FILE__, __func__, typeid(*this).name());
                }
            }
            bool descendants_stopped () { return !_child; }
            protected:
            typename S::bus_type _foreign = typename S::bus_type{};
            private:
            S* _child = nullptr;
        };
    }
}
