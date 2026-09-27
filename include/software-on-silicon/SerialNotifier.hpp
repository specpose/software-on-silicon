namespace SOS {
namespace Protocol {
    enum SequentialRequestInstruction : unsigned char {
        readstart = 0xFD,
        writestart = 0xFE, // body is transfer
        descriptorssend = 0xFF
    };
    enum SequentialInstructionResponse : unsigned char {
        readend = 0xFC,
        writeend = 0xFD, // body is transfer
        readfailed = 0xFF,
        writefailed = 0xFE,
        init = 0xFF
    };
    // more than 4 per In or Out: requires 2 UART, 2 baud, 10pin TTL or two Opcodes per baud. 4 per in or Out: id would fit, transfersend and desriptorsint byte not fit
    /*enum DMARequestInstruction : unsigned char {
        serviceinterrupted = 0xF9, // Out
        writeend = 0xFA, // Out
        readend = 0xFB, // Out
        writestart = 0xFC, // Out
        readstart = 0xFD, // Out
        checksync = 0xFE, // Out
        init = 0xFF // Out
    };
    enum DMAInstructionResponse : unsigned char {
        descriptorssend = 0xFB, // In, out of order. After init
        syncresponse = 0xFE, // In
        nocommand = 0xFF // In
    };*/
}
namespace MemoryView {
    class BusSequentialInstructions : public SOS::MemoryView::ComBus<UART2_BUFFER> { // byte1 instruction, byte2 DescriptorId, byte3 obj_size or error_code, MAX_OBJ_SIZE objectData
    public:
        BusSequentialInstructions(const typename UART2_BUFFER::iterator& inStart, const typename UART2_BUFFER::iterator& inEnd, const typename UART2_BUFFER::iterator& outStart, const typename UART2_BUFFER::iterator& outEnd)
            : SOS::MemoryView::ComBus<UART2_BUFFER> { inStart, inEnd, outStart, outEnd }
        {
        }
    };
    struct ComId : public SOS::MemoryView::TaskCable<unsigned char, 6> {
        using SOS::MemoryView::TaskCable<unsigned char, 6>::TaskCable;
        typename SOS::MemoryView::TaskCable<unsigned char, 6>::value_type& readStartId() { return std::get<0>(*this); }
        typename SOS::MemoryView::TaskCable<unsigned char, 6>::value_type& readEndId() { return std::get<1>(*this); }
        typename SOS::MemoryView::TaskCable<unsigned char, 6>::value_type& readFailedId() { return std::get<2>(*this); }
        typename SOS::MemoryView::TaskCable<unsigned char, 6>::value_type& writeStartId() { return std::get<3>(*this); }
        typename SOS::MemoryView::TaskCable<unsigned char, 6>::value_type& writeEndId() { return std::get<4>(*this); }
        typename SOS::MemoryView::TaskCable<unsigned char, 6>::value_type& writeFailedId() { return std::get<5>(*this); }
    };
    struct bus_sequential_tag {};
    class BusSequential : private std::array<std::atomic_flag, 10> {
    public:
        BusSequential()
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
        std::atomic_flag& getReadFailedNotifyRef() { return std::get<4>(*this); }
        std::atomic_flag& getWriteStartUpdatedRef() { return std::get<5>(*this); }
        std::atomic_flag& getWriteStartAcknowledgeRef() { return std::get<6>(*this); }
        std::atomic_flag& getWriteEndUpdatedRef() { return std::get<7>(*this); }
        std::atomic_flag& getWriteEndAcknowledgeRef() { return std::get<8>(*this); }
        std::atomic_flag& getWriteFailedNotifyRef() { return std::get<9>(*this); }
    };
    template <typename... Objects>
    struct BusSequentialShaker : bus<
    bus_sequential_tag,
    SOS::MemoryView::BusSequential,
    std::tuple<ComId>,
    bus_traits<Bus>::const_cables_type>
    {
        BusSequentialShaker()
            : objects{}
            , descriptors(cpp11_static_descriptors(objects))
        {
            std::get<0>(cables).readStartId().store(NUM_IDS);
            std::get<0>(cables).readEndId().store(NUM_IDS);
            std::get<0>(cables).readFailedId().store(NUM_IDS);
            std::get<0>(cables).writeStartId().store(NUM_IDS);
            std::get<0>(cables).writeEndId().store(NUM_IDS);
            std::get<0>(cables).writeFailedId().store(NUM_IDS);

        }
        signal_type signal;
        cables_type cables {};
        std::tuple<Objects...> objects;
        SOS::Protocol::DescriptorHelper descriptors; // descriptors has to outlive SequentialResolverDSP
        std::array<unsigned char, MAX_OBJ_SIZE> transfer {};
    };
    /*struct DMAInstructionCable : private SOS::MemoryView::TaskCable<unsigned char, 2> {
        using SOS::MemoryView::TaskCable<unsigned char, 2>::TaskCable;
        typename SOS::MemoryView::TaskCable<unsigned char, 2>::value_type& getOpcodeRef() { return std::get<0>(*this); }
        typename SOS::MemoryView::TaskCable<unsigned char, 2>::value_type& getWordRef() { return std::get<1>(*this); }
    };
    struct BusDMAInstructions : public SOS::MemoryView::BusShaker {
        using cables_type = std::tuple<DMAInstructionCable>;
        BusDMAInstructions() {
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
        std::atomic_flag& getSyncStartUpdatedRef() { return getUpdatedRef(); }
        std::atomic_flag& getSyncStartAcknowledgeRef() { return getAcknowledgeRef(); }
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
    };*/
    struct ResolverSwitch
    {
        ResolverSwitch() {
            read_ack.test_and_set();
            read_fault.test_and_set();
            write_ack.test_and_set();
            write_fault.test_and_set();
            sync_me.test_and_set();
        }
        SOS::MemoryView::Notify read_op{};
        std::atomic_flag read_ack = ATOMIC_FLAG_INIT;
        std::atomic_flag read_fault = ATOMIC_FLAG_INIT;
        SOS::MemoryView::Notify write_op{};
        std::atomic_flag write_ack = ATOMIC_FLAG_INIT;
        std::atomic_flag write_fault = ATOMIC_FLAG_INIT;
        std::atomic_flag sync_me = ATOMIC_FLAG_INIT;
    };
    class SwitchBoard : private SOS::MemoryView::Notify, private SOS::MemoryView::Ring, public std::array<ResolverSwitch, NUM_IDS>
    {
    public:
        SwitchBoard() : Notify(), Ring(), std::array<ResolverSwitch, NUM_IDS> {} {}
        std::atomic_flag& triggerResolve() { return getNotifyRef(); }
        std::atomic_flag& descriptorsUpdated() { return getRingRef(); }
        /*std::atomic_flag& readUpdated() { return getUpdatedRef(); }
        std::atomic_flag& readAcknowledge() { return getAcknowledgeRef(); }
        std::atomic_flag& writeUpdated() { return getAuxUpdatedRef(); }
        std::atomic_flag& writeAcknowledge() { return getAuxAcknowledgeRef(); }*/
    };
    /*struct Current : public SOS::MemoryView::TaskCable<std::size_t, 2> {
        using value_type = typename SOS::MemoryView::TaskCable<std::size_t, 2>::value_type;
        typename Current::value_type& currentRead() { return std::get<0>(*this); }
        typename Current::value_type& currentWrite() { return std::get<1>(*this); }
    };*/
    /*struct DescriptorInitObj : public TaskCable<void*, 1> {
        DescriptorInitObj() {
            std::get<0>(*this).store((void*)nullptr);
        }
    };
    struct DescriptorInitObj_Size : public TaskCable<unsigned long, 1> {
        DescriptorInitObj_Size() {
            std::get<0>(*this).store(MAX_OBJ_SIZE+1);
        }
    };*/
    struct bus_switchboard_tag { };
    struct SerialResolverBus : bus<
        bus_switchboard_tag,
        SOS::MemoryView::SwitchBoard,
        bus_traits<SOS::MemoryView::Bus>::cables_type,
        bus_traits<SOS::MemoryView::Bus>::const_cables_type>
    {
    //public:
        SerialResolverBus(SOS::Protocol::DescriptorHelper& helpers) : descriptors(helpers) {
            signal.triggerResolve().test_and_set();
            signal.descriptorsUpdated().test_and_set();
        }
        signal_type signal;
        //typename DescriptorInitObj::value_type& getObjPtr() { return std::get<0>(std::get<0>(cables)); }
        //typename DescriptorInitObj_Size::value_type& getObjSize() { return std::get<0>(std::get<1>(cables)); }
    //private:
        SOS::Protocol::DescriptorHelper& descriptors; // Reference causes Segfault
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
    struct ResolverStatus {
        ResolverStatus() {
            ready.test_and_set();
        }
        std::atomic_flag ready = ATOMIC_FLAG_INIT;
        bool result = true;
    };
    /*bool async_status(std::atomic_flag& fault, std::atomic_flag& ack)
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
    }*/
}
namespace Behavior {
    template <typename S, typename... Objects>
    class Async  : public SOS::Behavior::PassthruAsyncController<S,SOS::MemoryView::ComBus<UART1_BUFFER>> {
    public:
        using bus_type = SOS::MemoryView::ComBus<UART1_BUFFER>;
        Async(bus_type& uart1)
            : doubleBuffer{}
            , SOS::Behavior::PassthruAsyncController<S,SOS::MemoryView::ComBus<UART1_BUFFER>>(uart1)
        {
            // this->_foreign.descriptors = cpp11_static_descriptors(this->_foreign.objects);
            // this->_foreign.descriptors(this->_foreign.objects, make_integer_sequence<std::size_t, std::tuple_size<std::tuple<Objects...>>::value> {}); // integer_sequence: cpp14
            // apply(this->_foreign.descriptors, this->_foreign.objects); // fold expression: cpp17
            this->_foreign.signal.getWriteStartUpdatedRef().clear();
            this->_foreign.signal.getReadEndUpdatedRef().clear();
        }
        ~Async() {
            this->_foreign.descriptors.count = 0;
            for (std::size_t i = 0; i < NUM_IDS; ++i){
                this->_foreign.descriptors.arr[i].obj = (void*)nullptr;
                this->_foreign.descriptors.arr[i].obj_size = 0;
            }
        }
        virtual void event_loop() {
            if (!this->_foreign.signal.getReadEndUpdatedRef().test_and_set()) {
                auto id = std::get<0>(this->_foreign.cables).readEndId().load();
                if (id != NUM_IDS) {
                read_status[id].result = true;
                read_status[id].ready.clear();
                }
                this->_foreign.signal.getReadEndAcknowledgeRef().clear();
            }
        }
        bool read(std::size_t id) {
            if (!read_status[0].ready.test_and_set()) {
                if (read_status[0].result) {
                    return true;
                }
            }
            return false;
        }
        void write(std::size_t id) {
            if (!this->_foreign.signal.getWriteStartAcknowledgeRef().test_and_set()) {
                std::get<0>(this->_foreign.cables).writeStartId().store(id);
                this->_foreign.signal.getWriteStartUpdatedRef().clear();
            }
        }
    protected:
        std::array<SOS::Protocol::ResolverStatus, NUM_IDS> read_status {};
        std::array<SOS::Protocol::ResolverStatus, NUM_IDS> write_status {};
        std::tuple<Objects...> doubleBuffer;

    private:
        //std::tuple<Objects...> objects;
        //SOS::Protocol::DescriptorHelper descriptors;
    };
    /*template <typename... Objects>
    class DestructorGuard {
    public:
        DestructorGuard()
        : objects {}
        , descriptors(cpp11_static_descriptors(this->objects))
        {
        }
        virtual ~DestructorGuard() {
            this->descriptors.count = 0;
            for (std::size_t i = 0; i < NUM_IDS; ++i){
                this->descriptors.arr[i].obj = (void*)nullptr;
                this->descriptors.arr[i].obj_size = 0;
            }
            std::cout << "~DestructorGuard()" << std::endl;
        }
    protected:
        std::tuple<Objects...> objects;
        SOS::Protocol::DescriptorHelper descriptors; // descriptors has to outlive _sync
    };*/
    class SerialPreemptiveSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::BusSequentialShaker<TrueColorClass, DMA, DMA>; // REMOVE
        constexpr SerialPreemptiveSubController(typename bus_type::signal_type& signal)
        : SubController()
        , _intrinsic(signal)
        {
        }

    protected:
        bus_type::signal_type& _intrinsic;
    };
    template <typename S, typename OtherBus>
    class SerialDoublePassthruPreemptiveController : public Controller<S>, public Loop, protected SerialPreemptiveSubController {
    public:
        SerialDoublePassthruPreemptiveController(typename bus_type::signal_type& signal, typename S::bus_type& passThru, OtherBus& other)
        : Controller<S>()
        , Loop()
        , SerialPreemptiveSubController(signal)
        , _passthru(passThru.signal)
        , _other(other.signal)
        , _child(passThru, other)
        {
        }

    protected:
        typename S::bus_type::signal_type& _passthru;
        typename OtherBus::signal_type& _other;

    private:
        S _child;
    };
    template <typename S, typename OtherBus, typename... Objects>
    class SequentialResolverDSP : public SOS::Behavior::SerialDoublePassthruPreemptiveController<S, OtherBus> { // gcc bug: Debug target does not respect destruction order
    public:
        using bus_type = SOS::MemoryView::BusSequentialShaker<Objects...>;
        SequentialResolverDSP(bus_type& bus, SOS::MemoryView::ComBus<UART1_BUFFER>& other) // constexpr
            : result(bus)
            , sync(bus.descriptors)
            , SOS::Behavior::SerialDoublePassthruPreemptiveController<S, OtherBus>(result.signal, sync, other)
        {
            //while (this->_passthru.descriptorsUpdated().test_and_set())
            //    std::this_thread::yield();
            print_descriptors(result.descriptors);
            print_descriptors(sync.descriptors);
            this->_passthru.descriptorsUpdated().clear();
        }
        ~SequentialResolverDSP() { // Superclass, then members, then base class
            //while (this->_passthru.descriptorsUpdated().test_and_set())
            //    std::this_thread::yield();
            //this->_passthru.descriptorsUpdated().clear();
            std::cout << typeid(*this).name() << "ObjectReadsCanceled" << objectReadsCanceled << std::endl;
            std::cout << typeid(*this).name() << "ObjectWritesCanceled" << objectWritesCanceled << std::endl;
        }
        void event_loop()
        {
            if (!this->_passthru.triggerResolve().test_and_set()) {
                resolve(0);
            }
            if (!result.signal.getWriteStartUpdatedRef().test_and_set()) {
                auto id = std::get<0>(result.cables).writeStartId().load();
                if (id != NUM_IDS)
                    this->_passthru[id].sync_me.clear();
                result.signal.getWriteStartAcknowledgeRef().clear();
            }
            std::this_thread::yield();
        }
        void resolve(std::size_t id) {
            if (!this->_passthru[id].read_ack.test_and_set()) {
                if (this->_passthru[id].read_fault.test_and_set()) {
                    while (this->_passthru.descriptorsUpdated().test_and_set())
                        std::this_thread::yield();
                    if (!result.signal.getReadEndAcknowledgeRef().test_and_set()) {
                        std::get<0>(result.cables).readEndId().store(id);
                        result.signal.getReadEndUpdatedRef().clear();
                    }
                    //unsigned long i = 0;
                    //while (i < this->_foreign.descriptors[id].obj_size) {
                    //    if (_intrinsic[id].read_op.getNotifyRef().test_and_set()) {
                    //        i++;
                    //        doubleBuffer[id][i] = *reinterpret_cast<unsigned char*>(this->_foreign.descriptors[id].obj)+i;
                    //    } else {
                    //        i = 0;
                    //        break;
                    //    }
                    //    std::this_thread::yield();
                    //}
                    this->_passthru.descriptorsUpdated().clear();
                    //read_status[id].result = true;
                    //read_status[id].ready.clear();
                } else
                {
                    SFA::util::runtime_error(SFA::util::error_code::ServiceInterruptedByComShutdown, __FILE__, __func__, typeid(*this).name());
                    objectReadsCanceled++;
                    //read_status[id].result = false;
                    //read_status[id].ready.clear();
                }
            }
            if (!this->_passthru[id].write_ack.test_and_set()) {
                if (this->_passthru[id].write_fault.test_and_set()){
                    while (this->_passthru.descriptorsUpdated().test_and_set())
                        std::this_thread::yield();
                    //unsigned long i = 0;
                    //while (i < this->_foreign.descriptors[id].obj_size) {
                    //    if (_intrinsic[id].write_op.getNotifyRef().test_and_set()) {
                    //        i++;
                    //        doubleBuffer[id][i] = *reinterpret_cast<unsigned char*>(this->_foreign.descriptors[id].obj)+i;
                    //    } else {
                    //        i = 0;
                    //        break;
                    //    }
                    //    std::this_thread::yield();
                    //}
                    this->_passthru.descriptorsUpdated().clear();
                    //write_status[id].result = true;
                    //write_status[id].ready.clear();
                } else
                {
                    SFA::util::runtime_error(SFA::util::error_code::ObjectWriteCanceledByIncomingRead, __FILE__, __func__, typeid(*this).name());
                    objectWritesCanceled++;
                    //write_status[id].result = false;
                    //write_status[id].ready.clear();
                }
            }
        }

    protected:
        std::array<SOS::Protocol::ResolverStatus, NUM_IDS> read_status {};
        std::array<SOS::Protocol::ResolverStatus, NUM_IDS> write_status {};
        //std::array<std::array<unsigned char, MAX_OBJ_SIZE>, NUM_IDS> objects;
        SOS::MemoryView::SerialResolverBus sync;

    private:
        bus_type& result;
        std::size_t objectReadsCanceled = 0;
        std::size_t objectWritesCanceled = 0;
    };
    class SerialEventSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::SerialResolverBus;  // CUSTOM
        constexpr SerialEventSubController(typename bus_type::signal_type& signal)
        : SubController()
        , _intrinsic(signal)
        {
        }

    protected:
        bus_type::signal_type& _intrinsic;
    };
    template <typename OtherBus>
    class SerialPassthruEventDummy : public Loop, protected SerialEventSubController { // Useless: Refactoring only
    public:
        SerialPassthruEventDummy(typename bus_type::signal_type& signal, OtherBus& other)
        : Loop()
        , SerialEventSubController(signal)
        , _other(other.signal)
        {
        }

    protected:
        typename OtherBus::signal_type& _other;

    };
}
}