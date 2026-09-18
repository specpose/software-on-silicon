namespace SOS {
namespace MemoryView {
    class AsyncAndHandShake {
    public:
        AsyncAndHandShake()
        {
            aux_updated.test_and_set();
            aux_acknowledge.test_and_set();
        }
        std::atomic_flag& getAuxUpdatedRef() { return aux_updated; }
        std::atomic_flag& getAuxAcknowledgeRef() { return aux_acknowledge; }

    private:
        std::atomic_flag aux_updated {};
        std::atomic_flag aux_acknowledge {};
    };
    class NotifyAndHandShake : public SOS::MemoryView::Notify, public AsyncAndHandShake {
    public:
        NotifyAndHandShake()
            : SOS::MemoryView::Notify()
            , AsyncAndHandShake()
        {
        }
    };
    class DoubleHandShake : public SOS::MemoryView::HandShake, public AsyncAndHandShake {
    public:
        DoubleHandShake()
            : SOS::MemoryView::HandShake()
            , AsyncAndHandShake()
        {
        }
    };
    struct BusAsyncAndShaker : bus<
                                   bus_shaker_tag,
                                   SOS::MemoryView::AsyncAndHandShake,
                                   bus_traits<Bus>::cables_type,
                                   bus_traits<Bus>::const_cables_type> {
        signal_type signal;
    };
    struct bus_notifier_and_shaker_tag { };
    struct BusNotifierAndShaker : bus<
                                      bus_notifier_and_shaker_tag,
                                      SOS::MemoryView::NotifyAndHandShake,
                                      bus_traits<Bus>::cables_type,
                                      bus_traits<Bus>::const_cables_type> {
        signal_type signal;
    };
    struct bus_double_shaker_tag { };
    struct BusDoubleShaker : bus<
                                 bus_double_shaker_tag,
                                 SOS::MemoryView::DoubleHandShake,
                                 bus_traits<Bus>::cables_type,
                                 bus_traits<Bus>::const_cables_type> {
        signal_type signal;
    };
    template <typename ComIterator>
    struct ComSize : public SOS::MemoryView::ConstCable<ComIterator, 4> {
        using SOS::MemoryView::ConstCable<ComIterator, 4>::ConstCable;
        typename SOS::MemoryView::ConstCable<ComIterator, 4>::value_type& getInBufferStartRef() { return std::get<0>(*this); }
        typename SOS::MemoryView::ConstCable<ComIterator, 4>::value_type& getInBufferEndRef() { return std::get<1>(*this); }
        typename SOS::MemoryView::ConstCable<ComIterator, 4>::value_type& getOutBufferStartRef() { return std::get<2>(*this); }
        typename SOS::MemoryView::ConstCable<ComIterator, 4>::value_type& getOutBufferEndRef() { return std::get<3>(*this); }
    };
    template <typename ComIterator>
    struct ComOffset : public SOS::MemoryView::TaskCable<ComIterator, 2> {
        using SOS::MemoryView::TaskCable<ComIterator, 2>::TaskCable;
        typename SOS::MemoryView::TaskCable<ComIterator, 2>::value_type& getReadOffsetRef() { return std::get<0>(*this); }
        typename SOS::MemoryView::TaskCable<ComIterator, 2>::value_type& getWriteOffsetRef() { return std::get<1>(*this); }
    };
    template <typename ComBufferType>
    struct ComBus : public bus<
    bus_double_shaker_tag,
    SOS::MemoryView::DoubleHandShake,
    bus_traits<Bus>::cables_type,
    bus_traits<Bus>::const_cables_type> {
        signal_type signal;
        using const_cables_type = std::tuple<ComSize<typename ComBufferType::iterator>>;
        using cables_type = std::tuple<ComOffset<typename ComBufferType::difference_type>>;
        ComBus(const typename ComBufferType::iterator& inStart, const typename ComBufferType::iterator& inEnd, const typename ComBufferType::iterator& outStart, const typename ComBufferType::iterator& outEnd)
        : const_cables { ComSize<typename ComBufferType::iterator>({ inStart, inEnd, outStart, outEnd }) }
        {
            std::get<0>(cables).getReadOffsetRef() = 0;
            std::get<0>(cables).getWriteOffsetRef() = 0;
            if (std::distance(inStart, inEnd) < 1)
                SFA::util::logic_error(SFA::util::error_code::CombufferSizeIsMinimumWORDSIZE, __FILE__, __func__, typeid(*this).name());
            if (std::distance(outStart, outEnd) < 1)
                SFA::util::logic_error(SFA::util::error_code::CombufferSizeIsMinimumWORDSIZE, __FILE__, __func__, typeid(*this).name());
            if (std::distance(inStart, inEnd) != std::distance(outStart, outEnd))
                SFA::util::logic_error(SFA::util::error_code::CombufferInAndOutSizeNotEqual, __FILE__, __func__, typeid(*this).name());
        }
        cables_type cables {};
        const_cables_type const_cables;
    };
}
namespace Behavior {
    class Stoppable : public Loop { // FPGA are not stoppable
    public:
        Stoppable()
            : Loop()
        {
        }
        ~Stoppable()
        {
            // if (!is_finished())
            //     throw SFA::util::logic_error("stop() has not been called on Stoppable.", __FILE__, __func__);
            std::cout << typeid(*this).name() << "SN" << std::endl;
        }
    };
    class StoppableAsyncSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::BusAsyncAndShaker;
        StoppableAsyncSubController(typename bus_type::signal_type& signal)
            : SubController()
            , _intrinsic(signal)
        {
        }

    protected:
        bus_type::signal_type& _intrinsic;
    };
    class StoppableSimpleSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::BusNotifierAndShaker;
        StoppableSimpleSubController(typename bus_type::signal_type& signal)
            : SubController()
            , _intrinsic(signal)
        {
        }

    protected:
        bus_type::signal_type& _intrinsic;
    };
    class StoppableEventSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::BusDoubleShaker;
        StoppableEventSubController(typename bus_type::signal_type& signal)
            : SubController()
            , _intrinsic(signal)
        {
        }

    protected:
        bus_type::signal_type& _intrinsic;
    };
    // BootstrapDummies: Can start and stop themselves via the Stoppable interface and a mix-in _thread in the impl
    class StoppableAsyncDummy : public Stoppable, protected StoppableAsyncSubController {
    public:
        StoppableAsyncDummy(typename bus_type::signal_type& signal)
            : Stoppable()
            , StoppableAsyncSubController(signal)
        {
        }
    };
    class StoppableSimpleDummy : public Stoppable, protected StoppableSimpleSubController {
    public:
        StoppableSimpleDummy(typename bus_type::signal_type& signal)
            : Stoppable()
            , StoppableSimpleSubController(signal)
        {
        }
    };
    class StoppableEventDummy : public Stoppable, protected StoppableEventSubController {
    public:
        StoppableEventDummy(typename bus_type::signal_type& signal)
            : Stoppable()
            , StoppableEventSubController(signal)
        {
        }
    };
    // BootstrapController: Can start and stop their child and themselves via the Stoppable interface and a mix-in _thread in the impl
    template <typename S>
    class BootstrapAsyncController : public Controller<S>, public Stoppable, protected StoppableAsyncSubController {
    public:
        BootstrapAsyncController(typename bus_type::signal_type& signal)
            : Controller<S>()
            , Stoppable()
            , StoppableAsyncSubController(signal)
            , _child(new S { _foreign })
        {
        }
        ~BootstrapAsyncController()
        {
            if (_child) {
                // SFA::util::runtime_error(SFA::util::error_code::ChildHasToBeDeletedBeforeDestroyThread, __FILE__, __func__, typeid(*this).name());
                delete _child;
                _child = nullptr;
            }
        }
        void stop_descendants()
        {
            if (_child) {
                delete _child;
                _child = nullptr;
            } else {
                SFA::util::runtime_error(SFA::util::error_code::ChildHasAlreadyBeenDeleted, __FILE__, __func__, typeid(*this).name());
            }
        }
        bool descendants_stopped() { return !_child; }

    protected:
        typename S::bus_type _foreign {};

    private:
        S* _child = nullptr;
    };
    template <typename S>
    class BootstrapSimpleController : public Controller<S>, public Stoppable, protected StoppableSimpleSubController {
    public:
        BootstrapSimpleController(typename bus_type::signal_type& signal)
            : Controller<S>()
            , Stoppable()
            , StoppableSimpleSubController(signal)
            , _child(new S { _foreign })
        {
        }
        ~BootstrapSimpleController()
        {
            if (_child) {
                // SFA::util::runtime_error(SFA::util::error_code::ChildHasToBeDeletedBeforeDestroyThread, __FILE__, __func__, typeid(*this).name());
                delete _child;
                _child = nullptr;
            }
        }
        void stop_descendants()
        {
            if (_child) {
                delete _child;
                _child = nullptr;
            } else {
                SFA::util::runtime_error(SFA::util::error_code::ChildHasAlreadyBeenDeleted, __FILE__, __func__, typeid(*this).name());
            }
        }
        bool descendants_stopped() { return !_child; }

    protected:
        typename S::bus_type _foreign {};

    private:
        S* _child = nullptr;
    };
    template <typename S>
    class BootstrapEventController : public Controller<S>, public Stoppable, protected StoppableEventSubController {
    public:
        BootstrapEventController(typename bus_type::signal_type& signal)
            : Controller<S>()
            , Stoppable()
            , StoppableEventSubController(signal)
            , _child(new S { _foreign })
        {
        }
        ~BootstrapEventController()
        {
            if (_child) {
                // SFA::util::runtime_error(SFA::util::error_code::ChildHasToBeDeletedBeforeDestroyThread, __FILE__, __func__, typeid(*this).name());
                delete _child;
                _child = nullptr;
            }
        }
        void stop_descendants()
        {
            if (_child) {
                delete _child;
                _child = nullptr;
            } else {
                SFA::util::runtime_error(SFA::util::error_code::ChildHasAlreadyBeenDeleted, __FILE__, __func__, typeid(*this).name());
            }
        }
        bool descendants_stopped() { return !_child; }

    protected:
        typename S::bus_type _foreign {};

    private:
        S* _child = nullptr;
    };
}
}
