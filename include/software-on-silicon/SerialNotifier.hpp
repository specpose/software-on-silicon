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
    struct DestinationAndOrigin : public SOS::MemoryView::TaskCable<std::size_t, 5> {
        using value_type = SOS::MemoryView::TaskCable<std::size_t, 5>::value_type;
        DestinationAndOrigin()
            : SOS::MemoryView::TaskCable<std::size_t, 5> {}
        {
            std::fill(std::begin(*this), std::end(*this), 0);
        }
    };
    struct bus_dma_shaker_tag { };
    struct BusDMAShaker : bus<
                              bus_dma_shaker_tag,
                              SOS::MemoryView::DMAObjectShake,
                              std::tuple<DestinationAndOrigin>,
                              bus_traits<Bus>::const_cables_type> {
        signal_type signal;
        cables_type cables {};
        typename DestinationAndOrigin::value_type& readlockNotificationId() { return std::get<0>(std::get<0>(cables)); }
        typename DestinationAndOrigin::value_type& receiveNotificationId() { return std::get<1>(std::get<0>(cables)); }
        typename DestinationAndOrigin::value_type& sendNotificationId() { return std::get<2>(std::get<0>(cables)); }
        typename DestinationAndOrigin::value_type& syncStopId() { return std::get<3>(std::get<0>(cables)); }
        typename DestinationAndOrigin::value_type& syncStartId() { return std::get<4>(std::get<0>(cables)); }
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
    template <unsigned char N>
    struct Futures : public std::array<std::future<bool>, N> {
        Futures()
            : std::array<std::future<bool>, N> {}
        {
        }
    };
    // template<unsigned char N>
    // struct Promises : public std::array<std::promise<bool>,N> {
    //     //Promises(Objects&&... obj_refs) : std::tuple<std::promise<Objects&>...>{std::forward(obj_refs...)} {}
    //     Promises() : std::array<std::promise<bool>,N>{} {
    //         for (std::size_t i = 0; i < this->size(); ++i){
    //             (*this)[i].set_value(true);
    //         }
    //     }
    // };
    template <typename... Objects>
    struct SerialAsyncBus : public SOS::MemoryView::BusNotifier
    {
        SerialAsyncBus(SOS::Protocol::DescriptorHelper& descr) : descriptors(descr) {}
        SOS::Protocol::DescriptorHelper& descriptors;
    };
}
namespace Protocol {
    template <typename ObjectWithOwnership>
    class ObjectBusGenerator : public SOS::MemoryView::BusShaker {
    public:
        ObjectBusGenerator() = delete;
        ObjectBusGenerator(ObjectWithOwnership& obj)
        : SOS::MemoryView::BusShaker {}
        , obj{obj}
        {
        }
        ObjectWithOwnership& obj;
    };
    template <typename Object, std::size_t Sizeof = sizeof(Object), typename ArithmeticType = typename std::enable_if<Sizeof % 3 == 0 && Sizeof % 12 != 0 && Sizeof % 24 != 0 && Sizeof % 12288 != 0, unsigned char>::type>
    class CharBusGenerator : public SOS::MemoryView::BusShaker {
    public:
        CharBusGenerator() = delete;
        CharBusGenerator(Object& obj)
        : SOS::MemoryView::BusShaker {}
        , const_cables { Size<ArithmeticType*>(reinterpret_cast<ArithmeticType*>(&obj), reinterpret_cast<ArithmeticType*>(&obj) + Sizeof) }
        {
        }
        using const_cables_type = std::tuple<Size<ArithmeticType*>>;
        const_cables_type const_cables;
    };
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
    class SerialSimpleDummy : public Loop, protected SimpleSubController {
    public:
        using bus_type = SOS::MemoryView::SerialAsyncBus<Objects...>;
        constexpr SerialSimpleDummy(typename bus_type::signal_type& signal)
            : Loop()
            , SimpleSubController(signal)
        {
        }
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
            for (std::size_t i = 0; i < NUM_IDS; ++i) {
                //read_fault[i].test_and_set();
                //read_ack[i].test_and_set();
                //write_fault[i].test_and_set();
                //write_ack[i].test_and_set();
            }
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
                if (_dBus.descriptors[id].sync_me)
                    if (id == 1 || id == 2) {
                        std::cout << typeid(*this).name() << ": write of object id " << id << " canceled" << std::endl;
                        //write_fault[id].clear();
                        _dBus.descriptors[id].write_status[1] = true;
                        _dBus.descriptors[id].write_status[0] = false;
                    }
                _dBus.descriptors[id].sync_me = false;
                sync_registered_id[id] = false;
                _intrinsic.getSyncStopUpdatedRef().clear();
            }
            if (!_intrinsic.getReadStartAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.readlockNotificationId().load();
                read_id[id] = true;
                //read_ack[id].test_and_set();
                _dBus.descriptors[id].read_status[0] = true;
                //read_fault[id].test_and_set();
                _dBus.descriptors[id].read_status[1] = false;
                _intrinsic.getReadStartUpdatedRef().clear();
            }
            if (!_intrinsic.getReadEndAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.receiveNotificationId().load();
                read_id[id] = false;
                //read_ack[id].clear();
                _dBus.descriptors[id].read_status[0] = true;
                //read_fault[id].test_and_set();
                _dBus.descriptors[id].read_status[1] = true;
                _intrinsic.getReadEndUpdatedRef().clear();
                readOrWrite = true;
            }
            if (!_intrinsic.getWriteAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.sendNotificationId().load();
                write_id[id] = false;
                if (id == 1 || id == 2) {
                    std::cout << typeid(*this).name() << ": write of object id " << id << " succeeded" << std::endl;
                    //write_ack[id].clear();
                    _dBus.descriptors[id].write_status[1] = true;
                    _dBus.descriptors[id].write_status[0] = true;
                }
                _intrinsic.getWriteUpdatedRef().clear();
                readOrWrite = true;
            }
            if (!_intrinsic.getServiceInterruptedUpdatedRef().test_and_set()) {
                for (std::size_t id = 0; id < NUM_IDS; ++id) {
                    //if (read_ack[id].test_and_set())
                    if (_dBus.descriptors[id].read_status[0] && !_dBus.descriptors[id].read_status[1]) {
                        //read_fault[id].clear();
                        _dBus.descriptors[id].read_status[1] = true;
                        _dBus.descriptors[id].read_status[0] = false;
                    }
                }
                _intrinsic.getServiceInterruptedAcknowledgeRef().clear();
            }
            _dBus.signal.getNotifyRef().clear();
            if (readOrWrite) { // performance only?
                for (std::size_t i = 0; i < NUM_IDS; i++) {
                    if (_dBus.descriptors[i].sync_me && !sync_registered_id[i]) {
                        if (!_intrinsic.getSyncStartUpdatedRef().test_and_set()) {
                            _datasignals.syncStartId().store(i);
                            sync_registered_id[i] = true;
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
        std::bitset<NUM_IDS> read_id {};
        //std::array<std::atomic_flag, NUM_IDS> read_fault {};
        //std::array<std::atomic_flag, NUM_IDS> read_ack {};
        std::bitset<NUM_IDS> write_id {};
        //std::array<std::atomic_flag, NUM_IDS> write_fault {};
        //std::array<std::atomic_flag, NUM_IDS> write_ack {};
        std::bitset<NUM_IDS> sync_registered_id {};

    private:
        bus_type& _datasignals;
        typename S::bus_type& _dBus;
    };
}
}