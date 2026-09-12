namespace SOS {
namespace Protocol {
    // more than 4 per In or Out: requires 2 UART, 2 baud, 10pin TTL or two Opcodes per baud. 4 per in or Out: id would fit, transfersend and desriptorsint byte not fit
    enum SequentialLogicRequest : unsigned char {
        serviceinterrupted = 0xF9, // Out
        writeend = 0xFA, // Out
        readend = 0xFB, // Out
        writestart = 0xFC, // Out
        readstart = 0xFD, // Out
        checksync = 0xFE, // Out
        init = 0xFF // Out
    };
    enum SequentialLogicResponse : unsigned char {
        descriptorssend = 0xFB, // In, out of order. After init
        transfersend = 0xFC, // In, out of order. At writestart
        transferrequest = 0xFD, // In, out of order. After readend
        syncresponse = 0xFE, // In
        nocommand = 0xFF // In
    };
}
namespace MemoryView {
    struct SequentialCable : private SOS::MemoryView::TaskCable<unsigned char, 2> {
        using SOS::MemoryView::TaskCable<unsigned char, 2>::TaskCable;
        typename SOS::MemoryView::TaskCable<unsigned char, 2>::value_type& getOpcodeRef() { return std::get<0>(*this); }
        typename SOS::MemoryView::TaskCable<unsigned char, 2>::value_type& getWordRef() { return std::get<1>(*this); }
    };
    struct SequentialBus : public SOS::MemoryView::BusShaker { // FIX: ComBus
        using cables_type = std::tuple<SequentialCable>;
        SequentialBus() {
            std::get<0>(cables).getOpcodeRef().store(SOS::Protocol::nocommand);
            std::get<0>(cables).getWordRef().store(NUM_IDS);
        }
        cables_type cables {};
    };
    class DMAObjectShake : private SOS::MemoryView::HandShake, private std::array<std::atomic_flag, 12> {
    public:
        DMAObjectShake()
            : std::array<std::atomic_flag, 12> {}
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
            std::get<10>(*this).test_and_set();
            std::get<11>(*this).test_and_set();
        }
        std::atomic_flag& getReadStartUpdatedRef() { return std::get<0>(*this); }
        std::atomic_flag& getReadStartAcknowledgeRef() { return std::get<1>(*this); }
        std::atomic_flag& getReadEndUpdatedRef() { return std::get<2>(*this); }
        std::atomic_flag& getReadEndAcknowledgeRef() { return std::get<3>(*this); }
        std::atomic_flag& getServiceInterruptedUpdatedRef() { return std::get<4>(*this); }
        std::atomic_flag& getServiceInterruptedAcknowledgeRef() { return std::get<5>(*this); }
        std::atomic_flag& getWriteStartUpdatedRef() { return std::get<6>(*this); }
        std::atomic_flag& getWriteStartAcknowledgeRef() { return std::get<7>(*this); }
        std::atomic_flag& getWriteEndUpdatedRef() { return std::get<8>(*this); }
        std::atomic_flag& getWriteEndAcknowledgeRef() { return std::get<9>(*this); }
        std::atomic_flag& getSyncStopUpdatedRef() { return std::get<10>(*this);; }
        std::atomic_flag& getSyncStopAcknowledgeRef() { return std::get<11>(*this); }
        std::atomic_flag& getSyncStartUpdatedRef() { return updated; }
        std::atomic_flag& getSyncStartAcknowledgeRef() { return acknowledge; }
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
                              std::tuple<DestinationAndOrigin<6>>,
                              bus_traits<Bus>::const_cables_type> {
        signal_type signal;
        cables_type cables {};
        typename DestinationAndOrigin<6>::value_type& readlockNotificationId() { return std::get<0>(std::get<0>(cables)); }
        typename DestinationAndOrigin<6>::value_type& receivedNotificationId() { return std::get<1>(std::get<0>(cables)); }
        typename DestinationAndOrigin<6>::value_type& transferNotificationId() { return std::get<2>(std::get<0>(cables)); }
        typename DestinationAndOrigin<6>::value_type& sentNotificationId() { return std::get<3>(std::get<0>(cables)); }
        typename DestinationAndOrigin<6>::value_type& syncStopId() { return std::get<4>(std::get<0>(cables)); }
        typename DestinationAndOrigin<6>::value_type& syncStartId() { return std::get<5>(std::get<0>(cables)); }
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
        std::atomic_flag read_ack = ATOMIC_FLAG_INIT; // Processing and Async
        std::atomic_flag read_fault = ATOMIC_FLAG_INIT; // Processing and Async
        SOS::MemoryView::Pair write_op{}; // Serial and doubleBuffer
        std::atomic_flag write_ack = ATOMIC_FLAG_INIT; // Processing and Async
        std::atomic_flag write_fault = ATOMIC_FLAG_INIT; // Processing and Async
        std::atomic_flag sync_me = ATOMIC_FLAG_INIT; // Processing and Async
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
    struct Async {
        Async() {
            ready.test_and_set();
        }
        std::atomic_flag ready = ATOMIC_FLAG_INIT;
        bool result = true;
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
        }
        ~SerialSimpleDummy() {
            std::cout << typeid(*this).name() << "ObjectReadsCanceled" << objectReadsCanceled << std::endl;
            std::cout << typeid(*this).name() << "ObjectWritesCanceled" << objectWritesCanceled << std::endl;
        }
        void resolve(std::size_t id) {
            if (!dBus.signal[id].read_ack.test_and_set()) {
                if (dBus.signal[id].read_fault.test_and_set()) {
                    read[id].result = true;
                    read[id].ready.clear();
                } else
                {
                    //SFA::util::runtime_error(SFA::util::error_code::ServiceInterruptedByComShutdown, __FILE__, __func__, typeid(*this).name());
                    objectReadsCanceled++;
                    read[id].result = false;
                    read[id].ready.clear();
                }
            }
            if (!dBus.signal[id].write_ack.test_and_set()) {
                if (dBus.signal[id].write_fault.test_and_set()){
                    write[id].result = true;
                    write[id].ready.clear();
                } else
                {
                    SFA::util::runtime_error(SFA::util::error_code::ObjectWriteCanceledByIncomingRead, __FILE__, __func__, typeid(*this).name());
                    objectWritesCanceled++;
                    write[id].result = false;
                    write[id].ready.clear();
                }
            }
        }

    protected:
        std::array<SOS::Protocol::Async, NUM_IDS> read {};
        std::array<SOS::Protocol::Async, NUM_IDS> write {};

    //private:
        bus_type& dBus;
        std::size_t objectReadsCanceled = 0;
        std::size_t objectWritesCanceled = 0;
    };
    class SerialEventSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::SequentialBus; // Custom: bus_traits?
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
        using bus_type = SOS::MemoryView::SequentialBus; // Custom: bus_traits?
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
        using bus_type = SOS::MemoryView::SequentialBus;
        using SerialPassthruEventController<S>::_intrinsic;
        SerialProcessing(bus_type& bus, typename S::bus_type& passThru)
            : SOS::Behavior::SerialPassthruEventController<S>(bus.signal, passThru)
            , newBus(bus)
            , _dBus(passThru)
        {
            newBus.signal.getUpdatedRef().clear();
        }
        void event_loop()
        {
            if (init) {
            } else if (std::get<0>(transferIn)) {
                unsigned long i = 0;
                while (i < _dBus.descriptors[std::get<1>(transferIn)].obj_size) {
                    if (!newBus.signal.getAcknowledgeRef().test_and_set()) {
                        i++;
                        //doubleBuffer[std::get<1>(transferIn)][i] = std::get<0>(newBus.cables).getWordRef().load();
                        newBus.signal.getUpdatedRef().clear();
                    }
                    std::this_thread::yield();
                }
                std::get<0>(transferIn) = false;
                _dBus.signal[std::get<1>(transferIn)].read_ack.clear();
            } else if (std::get<0>(transferOut)) {
                unsigned long i = 0;
                while (i < _dBus.descriptors[std::get<1>(transferOut)].obj_size) {
                    if (!newBus.signal.getAcknowledgeRef().test_and_set()) {
                        i++;
                        //std::get<0>(newBus.cables).getWordRef().store(doubleBuffer[std::get<1>(transferOut)][i]);
                        newBus.signal.getUpdatedRef().clear();
                    }
                    std::this_thread::yield();
                }
                std::get<0>(transferOut) = false;
            } else {
                if (!newBus.signal.getAcknowledgeRef().test_and_set()) {
                    auto instruction = std::get<0>(newBus.cables).getOpcodeRef().load();
                    auto id = std::get<0>(newBus.cables).getWordRef().load();
                    std::get<0>(newBus.cables).getOpcodeRef().store(SOS::Protocol::nocommand);
                    std::get<0>(newBus.cables).getWordRef().store(NUM_IDS);
                    switch (instruction) {
                        case SOS::Protocol::init:
                            break;
                        case SOS::Protocol::checksync:
                            std::get<0>(newBus.cables).getOpcodeRef().store(SOS::Protocol::syncresponse);
                            if (!_dBus.signal[id].sync_me.test_and_set()) {
                                _dBus.signal[id].sync_me.clear();
                                std::get<0>(newBus.cables).getWordRef().store(id);
                            } else {
                                std::get<0>(newBus.cables).getWordRef().store(NUM_IDS);
                            }
                            break;
                        case SOS::Protocol::readstart:
                            //SFA::util::logic_error(SFA::util::error_code::ObjectSyncWasNeverRequested, __FILE__, __func__, typeid(*this).name());
                            if (!_dBus.signal[id].sync_me.test_and_set()) {
                                _dBus.signal[id].write_fault.clear();
                                _dBus.signal[id].write_ack.clear();
                            }
                            read_started_id[id] = true;
                            break;
                        case SOS::Protocol::readend:
                            read_started_id[id] = false;
                            std::get<0>(newBus.cables).getOpcodeRef().store(SOS::Protocol::transferrequest);
                            std::get<0>(newBus.cables).getWordRef().store(id);
                            transferIn = {true, id};
                            break;
                        case SOS::Protocol::writestart:
                            _dBus.signal[id].sync_me.test_and_set();
                            std::get<0>(newBus.cables).getOpcodeRef().store(SOS::Protocol::transfersend);
                            std::get<0>(newBus.cables).getWordRef().store(id);
                            transferOut = {true, id};
                            break;
                        case SOS::Protocol::writeend:
                            if (id == 1 || id == 2) {
                                std::cout << typeid(*this).name() << ": write of object id " << id << " succeeded" << std::endl;
                            }
                            _dBus.signal[id].write_ack.clear();
                            break;
                        case SOS::Protocol::serviceinterrupted:
                            for (std::size_t i = 0; i < _dBus.signal.size(); ++i) {
                                if (read_started_id[i]) {
                                    std::cout << typeid(*this).name() << ": object id " << i << " enters inaccessible state" << std::endl;
                                    _dBus.signal[i].read_fault.clear();
                                    read_started_id[i] = false;
                                }
                            }
                            break;
                    }
                    newBus.signal.getUpdatedRef().clear();
                }
            }
            std::this_thread::yield();
        }

    protected:
        std::bitset<NUM_IDS> read_started_id {};

    private:
        bus_type& newBus;
        typename S::bus_type& _dBus;
        std::array<std::array<unsigned char, MAX_OBJ_SIZE>, NUM_IDS> doubleBuffer{};
        std::tuple<bool, std::size_t> transferIn = {false, NUM_IDS};
        std::tuple<bool, std::size_t> transferOut = {false, NUM_IDS};
        bool init = false;
    };
}
}