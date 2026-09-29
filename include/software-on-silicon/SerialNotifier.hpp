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
    class SerialSequential : private std::array<std::atomic_flag, 11> {
    public:
        SerialSequential()
            : std::array<std::atomic_flag, 11> {}
        {
            std::get<0>(*this).test_and_set();
            std::get<1>(*this).test_and_set();
            std::get<2>(*this).test_and_set();
            std::get<3>(*this).clear();
            std::get<4>(*this).test_and_set();
            std::get<5>(*this).test_and_set();
            std::get<6>(*this).clear();
            std::get<7>(*this).test_and_set();
            std::get<8>(*this).test_and_set();
            std::get<9>(*this).test_and_set();
            std::get<10>(*this).test_and_set();
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
        std::atomic_flag& getWriteInProgressNotifyRef() { return std::get<10>(*this); }
    };
    struct bus_sequential_tag {};
    struct BusSerialSequential : bus<
    bus_sequential_tag,
    SOS::MemoryView::SerialSequential,
    std::tuple<ComId>,
    bus_traits<Bus>::const_cables_type>
    {
        BusSerialSequential()
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
        std::array<unsigned char, MAX_OBJ_SIZE> transferReadEndIn {};
        std::array<unsigned char, MAX_OBJ_SIZE> transferWriteStartOut {};
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
    template <typename... Objects>
    struct SerialResolverBus : bus<
        bus_switchboard_tag,
        SOS::MemoryView::SwitchBoard,
        bus_traits<SOS::MemoryView::Bus>::cables_type,
        bus_traits<SOS::MemoryView::Bus>::const_cables_type>
    {
    //public:
        SerialResolverBus()
        {
            signal.triggerResolve().test_and_set();
            signal.descriptorsUpdated().test_and_set();
        }
        signal_type signal;
        //typename DescriptorInitObj::value_type& getObjPtr() { return std::get<0>(std::get<0>(cables)); }
        //typename DescriptorInitObj_Size::value_type& getObjSize() { return std::get<0>(std::get<1>(cables)); }
    //private:
        SOS::Protocol::DescriptorHelper descriptors;
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
    class REST  : public SOS::Behavior::PassthruAsyncController<S,SOS::MemoryView::ComBus<UART1_BUFFER>> {
    public:
        using bus_type = SOS::MemoryView::ComBus<UART1_BUFFER>;
        REST(bus_type& uart1)
            : SOS::Behavior::PassthruAsyncController<S,SOS::MemoryView::ComBus<UART1_BUFFER>>(uart1)
        {

        }
        ~REST() {

        }
        virtual void event_loop() {
            //COMMAND
            if (!this->_foreign.signal.getReadEndUpdatedRef().test_and_set()) { // receiver end
                auto id = std::get<0>(this->_foreign.cables).readEndId().load();
                if (id < NUM_IDS) {
                    read_status[id].result = true;
                    read_status[id].ready.clear();
                } else {
                    SFA::util::logic_error(SFA::util::error_code::IllegalReadEndId, __FILE__, __func__, typeid(*this).name());
                }
                this->_foreign.signal.getReadEndAcknowledgeRef().clear();
            }
        }
        //does not receive any data, informs when getReadEnd
        bool read_event(std::size_t id) {
            if (!read_status[0].ready.test_and_set())
                return true;
            return false;
        }
        bool read_success(std::size_t id) {
            if (read_status[0].result)
                return true;
            return false;
        }
        /*bool write_event(std::size_t id) {
            if (!write_status[0].ready.test_and_set())
                return true;
            return false;
        }
        bool write_success(std::size_t id) {
            if (write_status[0].result)
                return true;
            return false;
        }*/
        //getReadStartgets a copy of cache blocking, WAITS if readLock
        /*template <unsigned char id>
        char read(typename std::tuple_element<id, typename std::tuple<Objects...>>::type& readinto){
            if (!read_status[id].ready.test_and_set()) {
                if (read_status[id].result) {
                    //readinto = transfer;
                    return 1;
                } else {
                    return -1;
                }
            } else {
                return 0;
            }
        }*/
        //getWriteStart writes a copy to cache, blocking, HARDFAILS if transfer: getWriteInProgress
        template <unsigned char id>
        void write(typename std::tuple_element<id, typename std::tuple<Objects...>>::type& writefrom) {
            while (this->_foreign.signal.getWriteStartAcknowledgeRef().test_and_set()) // emitter side
                std::this_thread::yield();
            std::get<0>(this->_foreign.cables).writeStartId().store(id);
            // FIX
            //while (this->_passthru.descriptorsUpdated().test_and_set())
            //    std::this_thread::yield();
            for (std::size_t i = 0; i < sizeof(typename std::tuple_element<id, typename std::tuple<Objects...>>::type); ++i)
                this->_foreign.transferWriteStartOut[i] = *reinterpret_cast<unsigned char*>(&writefrom);
            //this->_passthru.descriptorsUpdated().clear();
            this->_foreign.signal.getWriteStartUpdatedRef().clear();
        }
        //does a write and checks for write_event with write_success or incomingReadCanceledWrite
        //await_write
    protected:
        std::array<SOS::Protocol::ResolverStatus, NUM_IDS> read_status {};
        std::array<SOS::Protocol::ResolverStatus, NUM_IDS> write_status {};

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
    class SerialSequentialSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::BusSerialSequential;
        constexpr SerialSequentialSubController(SOS::MemoryView::SerialSequential& signal)
        : SubController()
        , _intrinsic(signal)
        {
        }

    protected:
        SOS::MemoryView::SerialSequential& _intrinsic;
    };
    template <typename S, typename OtherBus>
    class SerialPassthruSerialSequentialController : public Controller<S>, public Loop, protected SerialSequentialSubController {
    public:
        SerialPassthruSerialSequentialController(SOS::MemoryView::SerialSequential& signal, OtherBus& other)
        : Controller<S>()
        , Loop()
        , SerialSequentialSubController(signal)
        , _other(other.signal)
        , _child(_foreign, other)
        {
        }

    protected:
        typename S::bus_type _foreign {};
        typename OtherBus::signal_type& _other;

    private:
        S _child;
    };
    template <typename S, typename OtherBus, typename... Objects>
    class SequentialResolverDSP : public SOS::Behavior::SerialPassthruSerialSequentialController<S, OtherBus> { // gcc bug: Debug target does not respect destruction order
    public:
        using bus_type = SOS::MemoryView::BusSerialSequential;
        SequentialResolverDSP(bus_type& bus, SOS::MemoryView::ComBus<UART1_BUFFER>& other) // constexpr
            : result(bus)
            , SOS::Behavior::SerialPassthruSerialSequentialController<S, OtherBus>(result.signal, other)
        {
            this->_foreign.descriptors = cpp11_static_descriptors(objects);
            // this->_foreign.descriptors(objects, make_integer_sequence<std::size_t, std::tuple_size<std::tuple<Objects...>>::value> {}); // integer_sequence: cpp14
            // apply(this->_foreign.descriptors, objects); // fold expression: cpp17
            print_descriptors(this->_foreign.descriptors);
            this->_foreign.signal.descriptorsUpdated().clear();
        }
        ~SequentialResolverDSP() { // Superclass, then members, then base class
            while (this->_foreign.signal.descriptorsUpdated().test_and_set())
                std::this_thread::yield();
            this->_foreign.descriptors.count = 0;
            for (std::size_t i = 0; i < NUM_IDS; ++i){
                this->_foreign.descriptors.arr[i].obj = (void*)nullptr;
                this->_foreign.descriptors.arr[i].obj_size = 0;
            }
            this->_foreign.signal.descriptorsUpdated().clear();
            std::cout << typeid(*this).name() << "ObjectReadsCanceled" << objectReadsCanceled << std::endl;
            std::cout << typeid(*this).name() << "ObjectWritesCanceled" << objectWritesCanceled << std::endl;
        }
        void event_loop()
        {
            if (!this->_foreign.signal.triggerResolve().test_and_set()) {
                for (std::size_t i = 0; i < this->_foreign.descriptors.size(); ++i)
                    resolve(i);
            }
            //COMMAND
            if (readend_command_queued) {
                while (result.signal.getReadEndAcknowledgeRef().test_and_set()) // emitter side
                    std::this_thread::yield();
                auto id = std::get<0>(result.cables).readEndId().load(); // from local
                if (id < NUM_IDS) {
                    while (this->_foreign.signal.descriptorsUpdated().test_and_set())
                        std::this_thread::yield();
                    for (std::size_t i = 0; i < this->_foreign.descriptors[id].obj_size; ++i)
                        result.transferReadEndIn[i] = *reinterpret_cast<unsigned char*>(this->_foreign.descriptors[id].obj);
                    this->_foreign.signal.descriptorsUpdated().clear();
                }
                result.signal.getReadEndUpdatedRef().clear();
                readend_command_queued = false;
            }
            //if (writend_command_queued)
            //    if (!result.signal.getWriteEndAcknowledgeRef().test_and_set()) {
            //        result.signal.getWriteEndUpdatedRef().clear();
            //    }
            if (!result.signal.getWriteStartUpdatedRef().test_and_set()) { // receiver end
                auto id = std::get<0>(result.cables).writeStartId().load();
                if (id < NUM_IDS)
                    if (!pending_write_request[id]) {
                        while (this->_foreign.signal.descriptorsUpdated().test_and_set())
                            std::this_thread::yield();
                        for (std::size_t i = 0; i < this->_foreign.descriptors[id].obj_size; ++i)
                            *reinterpret_cast<unsigned char*>(this->_foreign.descriptors[id].obj) = result.transferWriteStartOut[i];
                        this->_foreign.signal.descriptorsUpdated().clear();
                        pending_write_request[id] = true;
                        this->_foreign.signal[id].sync_me.clear();
                    } else {
                        SFA::util::runtime_error(SFA::util::error_code::PendingWriteRequest, __FILE__, __func__, typeid(*this).name());
                    }
                else
                    SFA::util::logic_error(SFA::util::error_code::IllegalWriteStartId, __FILE__, __func__, typeid(*this).name());
                result.signal.getWriteStartAcknowledgeRef().clear();
            }
            std::this_thread::yield();
        }
        void resolve(std::size_t id) {
            if (!this->_foreign.signal[id].read_ack.test_and_set()) {
                if (this->_foreign.signal[id].read_fault.test_and_set()) {
                    std::get<0>(result.cables).readEndId().store(id);
                    readend_command_queued = true;
                } else
                {
                    SFA::util::runtime_error(SFA::util::error_code::ServiceInterruptedByComShutdown, __FILE__, __func__, typeid(*this).name());
                    objectReadsCanceled++;
                    //std::get<0>(result.cables).readFailedId().store(id);
                    //readfault_command_queued = true;
                }
            }
            if (!this->_foreign.signal[id].write_ack.test_and_set()) {
                if (this->_foreign.signal[id].write_fault.test_and_set()){
                    std::get<0>(result.cables).writeEndId().store(id);
                    writend_command_queued = true;
                    pending_write_request[id] = false;
                } else
                {
                    SFA::util::runtime_error(SFA::util::error_code::ObjectWriteCanceledByIncomingRead, __FILE__, __func__, typeid(*this).name());
                    objectWritesCanceled++;
                    //std::get<0>(result.cables).writeFailedId().store(id);
                    //writefault_command_queued = true;
                }
            }
        }

    protected:
        bool writend_command_queued = false;
        bool readend_command_queued = false;
        std::array<bool, NUM_IDS> readfault_command_queued { false };
        std::array<bool, NUM_IDS> pending_write_request { false };

    private:
        bus_type& result;
        std::size_t objectReadsCanceled = 0;
        std::size_t objectWritesCanceled = 0;

        std::tuple<Objects...> objects {}; // REST
        //std::array<std::array<unsigned char, MAX_OBJ_SIZE>, NUM_IDS> objects; // AsyncIO
    };
    template <typename... Objects>
    class SerialEventSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::SerialResolverBus<Objects...>;  // CUSTOM
        constexpr SerialEventSubController(typename bus_type::signal_type& signal)
        : SubController()
        , _intrinsic(signal)
        {
        }

    protected:
        typename bus_type::signal_type& _intrinsic;
    };
    template <typename OtherBus, typename... Objects>
    class SerialPassthruEventDummy : public Loop, protected SerialEventSubController<Objects...> { // Useless: Refactoring only
    public:
        using bus_type = typename SerialEventSubController<Objects...>::bus_type;
        SerialPassthruEventDummy(typename bus_type::signal_type& signal, OtherBus& other)
        : Loop()
        , SerialEventSubController<Objects...>(signal)
        , _other(other.signal)
        {
        }

    protected:
        typename OtherBus::signal_type& _other;

    };
}
}