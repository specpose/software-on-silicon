namespace SOS {
namespace MemoryView {
    class DMAObjectShake : private SOS::MemoryView::HandShake, private std::array<std::atomic_flag, 10> {
    public:
        DMAObjectShake()
            : std::array<std::atomic_flag, 10> {}
        {
            std::get<0>(*this).test_and_set();
            std::get<1>(*this).test_and_set();
            std::get<2>(*this).test_and_set();
            std::get<3>(*this).test_and_set();
            std::get<4>(*this).test_and_set();
            std::get<5>(*this).test_and_set();
            std::get<6>(*this).test_and_set();
            std::get<7>(*this).test_and_set();
            std::get<8>(*this).test_and_set();
            std::get<9>(*this).test_and_set();
        }
        std::atomic_flag& getReadStartUpdatedRef() { return std::get<0>(*this); }
        std::atomic_flag& getReadStartAcknowledgeRef() { return std::get<1>(*this); }
        std::atomic_flag& getReadEndUpdatedRef() { return std::get<2>(*this); }
        std::atomic_flag& getReadEndAcknowledgeRef() { return std::get<3>(*this); }
        std::atomic_flag& getServiceInterruptedUpdatedRef() { return std::get<4>(*this); }
        std::atomic_flag& getServiceInterruptedAcknowledgeRef() { return std::get<5>(*this); }
        std::atomic_flag& getWriteUpdatedRef() { return std::get<6>(*this); }
        std::atomic_flag& getWriteAcknowledgeRef() { return std::get<7>(*this); }
        std::atomic_flag& getSyncStartUpdatedRef() { return std::get<8>(*this); }
        std::atomic_flag& getSyncStartAcknowledgeRef() { return std::get<9>(*this); }
        std::atomic_flag& getSyncStopUpdatedRef() { return updated; }
        std::atomic_flag& getSyncStopAcknowledgeRef() { return acknowledge; }
    };
    template<std::size_t N>
    struct DestinationAndOrigin : public SOS::MemoryView::TaskCable<std::size_t, N> {
        using value_type = typename SOS::MemoryView::TaskCable<std::size_t, N>::value_type;
        DestinationAndOrigin()
            : SOS::MemoryView::TaskCable<std::size_t, N> {}
        {
            std::fill(std::begin(*this), std::end(*this), 0);
        }
    };
    struct bus_dma_shaker_tag { };
    struct BusDMAShaker : bus<
                              bus_dma_shaker_tag,
                              SOS::MemoryView::DMAObjectShake,
                              std::tuple<DestinationAndOrigin<5>>,
                              bus_traits<Bus>::const_cables_type> {
        signal_type signal;
        cables_type cables {};
        typename DestinationAndOrigin<5>::value_type& readlockNotificationId() { return std::get<0>(std::get<0>(cables)); }
        typename DestinationAndOrigin<5>::value_type& receiveNotificationId() { return std::get<1>(std::get<0>(cables)); }
        typename DestinationAndOrigin<5>::value_type& sendNotificationId() { return std::get<2>(std::get<0>(cables)); }
        typename DestinationAndOrigin<5>::value_type& syncStopId() { return std::get<3>(std::get<0>(cables)); }
        typename DestinationAndOrigin<5>::value_type& syncStartId() { return std::get<4>(std::get<0>(cables)); }
    };
    struct DMAObjectAsyncSwitch
    {
        DMAObjectAsyncSwitch() {
            read_ack.test_and_set();
            read_fault.test_and_set();
            write_ack.test_and_set();
            write_fault.test_and_set();
            sync_me.test_and_set();
        }
        SOS::MemoryView::Pair read_op{}; // Serial and doubleBuffer
        std::atomic_flag read_ack; // Processing and Async
        std::atomic_flag read_fault; // Processing and Async
        SOS::MemoryView::Pair write_op{}; // Serial and doubleBuffer
        std::atomic_flag write_ack; // Processing and Async
        std::atomic_flag write_fault; // Processing and Async
        std::atomic_flag sync_me; // Processing and Async
    };
    class SwitchBoard : public SOS::MemoryView::Notify, public std::array<DMAObjectAsyncSwitch, NUM_IDS>
    {
    public:
        SwitchBoard() : Notify(), std::array<DMAObjectAsyncSwitch, NUM_IDS> {} {}
    };
    struct serial_async_tag { };
    template <typename... Objects>
    struct SerialAsyncBus : bus<
        serial_async_tag,
        SOS::MemoryView::SwitchBoard,
        bus_traits<SOS::MemoryView::Bus>::cables_type,
        bus_traits<SOS::MemoryView::Bus>::const_cables_type>
    {
        SerialAsyncBus(SOS::Protocol::DescriptorHelper& descr) : descriptors(descr) {}
        signal_type signal;
        SOS::Protocol::DescriptorHelper& descriptors;
    };
}
namespace Protocol {
    enum state : unsigned char {
        reserved2 = 0x00, // Clock divider?
        sighup = 0x3D,
        poweron = 0x3E,
        idle = 0x3B,
        shutdown = 0x3C,
        reserved = 0x3F // 8bit LEQ instruction?
    };
    template <typename ArithmeticType>
    struct Size : public SOS::MemoryView::ConstCable<ArithmeticType, 2> {
        Size(const ArithmeticType First, const ArithmeticType Second)
        : SOS::MemoryView::ConstCable<ArithmeticType, 2>({ First, Second })
        {
        }
    };
}
namespace MemoryView {
    //template <unsigned char N>
    //struct Futures : public std::array<std::future<bool>, N> {
    //    Futures()
    //        : std::array<std::future<bool>, N> {}
    //    {
    //    }
    //};
    // template<unsigned char N>
    // struct Promises : public std::array<std::promise<bool>,N> {
    //     //Promises(Objects&&... obj_refs) : std::tuple<std::promise<Objects&>...>{std::forward(obj_refs...)} {}
    //     Promises() : std::array<std::promise<bool>,N>{} {
    //         for (std::size_t i = 0; i < this->size(); ++i){
    //             (*this)[i].set_value(true);
    //         }
    //     }
    // };
}
namespace Protocol {
    //template <typename ObjectWithOwnership>
    //class ObjectBusGenerator : public SOS::MemoryView::BusShaker {
    //public:
    //    ObjectBusGenerator() = delete;
    //    ObjectBusGenerator(ObjectWithOwnership& obj)
    //    : SOS::MemoryView::BusShaker {}
    //    , obj{obj}
    //    {
    //    }
    //    ObjectWithOwnership& obj;
    //};
    //template <typename Object, std::size_t Sizeof = sizeof(Object), typename ArithmeticType = typename std::enable_if<Sizeof % 3 == 0 && Sizeof % 12 != 0 && Sizeof % 24 != 0 && Sizeof % 12288 != 0, unsigned char>::type>
    //class CharBusGenerator : public SOS::MemoryView::BusShaker {
    //public:
    //    CharBusGenerator() = delete;
    //    CharBusGenerator(Object& obj)
    //    : SOS::MemoryView::BusShaker {}
    //    , const_cables { Size<ArithmeticType*>(reinterpret_cast<ArithmeticType*>(&obj), reinterpret_cast<ArithmeticType*>(&obj) + Sizeof) }
    //    {
    //    }
    //    using const_cables_type = std::tuple<Size<ArithmeticType*>>;
    //    const_cables_type const_cables;
    //};
    bool async_status(std::atomic_flag& fault, std::atomic_flag& ack)
    {
        bool exit = false;
        bool result = false;
        while (!exit) {
            if (!fault.test_and_set()) {
                result = false;
                exit = true;
            }
            if (!ack.test_and_set()) {
                result = true;
                exit = true;
            }
            std::this_thread::yield();
        }
        return result;
    }
}
namespace Behavior {
    template <typename... Objects>
    class SerialSimpleSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::SerialAsyncBus<Objects...>;
        constexpr SerialSimpleSubController(typename bus_type::signal_type& signal)
        : SubController()
        , _intrinsic(signal)
        {
        }

    protected:
        typename bus_type::signal_type& _intrinsic;
    };
    template <typename... Objects>
    class SerialSimpleDummy : public Loop, protected SerialSimpleSubController<Objects...> {
    public:
        using bus_type = SOS::MemoryView::SerialAsyncBus<Objects...>;
        using SerialSimpleSubController<Objects...>::_intrinsic;
        SerialSimpleDummy(bus_type& bus) // constexpr
            : Loop()
            , SerialSimpleSubController<Objects...>(bus.signal)
            , dBus(bus)
        {
            for (std::size_t i = 0; i < read.size(); i++)
                read[i].test_and_set();
            for (std::size_t i = 0; i < write.size(); i++)
                write[i].test_and_set();
        }
        void transfer(std::size_t id) {
            if (!dBus.signal[id].read_ack.test_and_set()) {
                if (dBus.signal[id].read_fault.test_and_set()) {
                    unsigned long i = 0;
                    while (i < dBus.descriptors[id].obj_size) {
                        if (_intrinsic[id].read_op.getFirstRef().test_and_set()) {
                            i++;
                            doubleBuffer[id][i] = *reinterpret_cast<unsigned char*>(dBus.descriptors[id].obj)+i;
                        } else {
                            while (_intrinsic[id].read_op.getSecondRef().test_and_set())
                                std::this_thread::yield();
                            i = 0;
                        }
                        std::this_thread::yield();
                    }
                    read[id].clear();
                } else
                {
                    SFA::util::runtime_error(SFA::util::error_code::ServiceInterruptedByComShutdown, __FILE__, __func__, typeid(*this).name());
                }
            }
            if (!dBus.signal[id].write_ack.test_and_set()) {
                if (dBus.signal[id].write_fault.test_and_set()){
                    unsigned long i = 0;
                    while (i < dBus.descriptors[id].obj_size) {
                        if (_intrinsic[id].read_op.getFirstRef().test_and_set()) {
                            i++;
                            auto tmp = reinterpret_cast<unsigned char*>(dBus.descriptors[id].obj)+i;
                            *reinterpret_cast<unsigned char*>(tmp) = doubleBuffer[id][i];
                        } else {
                            while (_intrinsic[id].read_op.getSecondRef().test_and_set())
                                std::this_thread::yield();
                            i = 0;
                        }
                    }
                    write[id].clear();
                } else
                {
                    SFA::util::runtime_error(SFA::util::error_code::ObjectWriteCanceledByIncomingRead, __FILE__, __func__, typeid(*this).name());
                }
            }
        }

    protected:
        std::array<std::atomic_flag, NUM_IDS> read {};
        std::array<std::atomic_flag, NUM_IDS> write {};
        std::array<std::array<unsigned char, MAX_OBJ_SIZE>, NUM_IDS> doubleBuffer{};

    //private:
        bus_type& dBus;
    };
    class SerialEventSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::BusDMAShaker; // Custom: bus_traits?
        constexpr SerialEventSubController(typename bus_type::signal_type& signal)
            : SubController()
            , _intrinsic(signal)
        {
        }

    protected:
        bus_type::signal_type& _intrinsic;
    };
    template <typename S, typename... Others>
    class SerialPassthruEventController : public Controller<S>, public Loop, protected SerialEventSubController {
    public:
        using bus_type = SOS::MemoryView::BusDMAShaker; // Custom: bus_traits?
        SerialPassthruEventController(typename bus_type::signal_type& signal, typename S::bus_type& passThru, Others&... args)
            : Controller<S>()
            , Loop()
            , SerialEventSubController(signal)
            , _foreign(passThru)
            , _child(_foreign, args...)
        {
        }

    protected:
        typename S::bus_type& _foreign;

    private:
        S _child;
    };
    template <typename S>
    class SerialProcessing : public SOS::Behavior::SerialPassthruEventController<S> {
    public:
        using bus_type = SOS::MemoryView::BusDMAShaker;
        using SerialPassthruEventController<S>::_intrinsic;
        SerialProcessing(bus_type& bus, typename S::bus_type& passThru)
            : SOS::Behavior::SerialPassthruEventController<S>(bus.signal, passThru)
            , _datasignals(bus)
            , _dBus(passThru)
        {
            for (std::size_t i = 0; i < sync_registered_id.size(); i++)
                sync_registered_id[i] = false;
            _intrinsic.getSyncStartUpdatedRef().clear();
            readOrWrite = true; // one sync is enough to trigger a read or write hook
            _intrinsic.getSyncStopUpdatedRef().clear();
            _intrinsic.getWriteUpdatedRef().clear();
            _intrinsic.getReadEndUpdatedRef().clear();
            _intrinsic.getServiceInterruptedUpdatedRef().clear();
        }
        void event_loop()
        {
            if (!_intrinsic.getSyncStopAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.syncStopId().load();
                if (sync_registered_id[id]) {
                    if (id == 1 || id == 2) {
                        std::cout << typeid(*this).name() << ": write of object id " << id << " canceled" << std::endl;
                    }
                    sync_registered_id[id] = false;
                    _dBus.signal[id].write_fault.clear();
                } else {
                    // Error
                }
                _intrinsic.getSyncStopUpdatedRef().clear();
            }
            /*if (!_intrinsic.getReadStartAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.readlockNotificationId().load();
                read_started_id[id] = true;
                _intrinsic.getReadStartUpdatedRef().clear();
            }*/
            if (!_intrinsic.getReadEndAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.receiveNotificationId().load();
                read_started_id[id] = false;
                _dBus.signal[id].read_ack.clear();
                _intrinsic.getReadEndUpdatedRef().clear();
                readOrWrite = true;
            }
            if (!_intrinsic.getWriteAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.sendNotificationId().load();
                if (id == 1 || id == 2) {
                    std::cout << typeid(*this).name() << ": write of object id " << id << " succeeded" << std::endl;
                }
                _dBus.signal[id].write_ack.clear();
                _intrinsic.getWriteUpdatedRef().clear();
                readOrWrite = true;
            }
            /*if (!_intrinsic.getServiceInterruptedUpdatedRef().test_and_set()) {
                for (std::size_t id = 0; id < _dBus.signal.size(); ++id) {
                    if (read_started_id[id]) {
                        std::cout << typeid(*this).name() << ": object id " << id << " enters illegal state" << std::endl;
                        _dBus.signal[id].read_fault.clear();
                        read_started_id[id] = false;
                    }
                }
                _intrinsic.getServiceInterruptedAcknowledgeRef().clear();
            }*/
            if (readOrWrite) { // performance only?
                for (std::size_t id = 0; id < _dBus.signal.size(); id++) {
                    if (!_dBus.signal[id].sync_me.test_and_set() && !sync_registered_id[id]) {
                        if (!_intrinsic.getSyncStartUpdatedRef().test_and_set()) {
                            std::cout << "Store";
                            _datasignals.syncStartId().store(id);
                            sync_registered_id[id] = true;
                            _intrinsic.getSyncStartAcknowledgeRef().clear();
                        }
                        break;
                    }
                }
                readOrWrite = false;
            }
            std::this_thread::yield();
        }

    protected:
        bool readOrWrite = false;
        std::bitset<NUM_IDS> read_started_id {};
        std::bitset<NUM_IDS> sync_registered_id {};

    private:
        bus_type& _datasignals;
        typename S::bus_type& _dBus;
    };
}
}