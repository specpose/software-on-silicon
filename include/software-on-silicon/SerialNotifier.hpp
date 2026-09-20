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
    struct SequentialBus : public SOS::MemoryView::BusShaker {
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
        SOS::MemoryView::Notify read_op{};
        std::atomic_flag read_ack = ATOMIC_FLAG_INIT;
        std::atomic_flag read_fault = ATOMIC_FLAG_INIT;
        SOS::MemoryView::Notify write_op{};
        std::atomic_flag write_ack = ATOMIC_FLAG_INIT;
        std::atomic_flag write_fault = ATOMIC_FLAG_INIT;
        std::atomic_flag sync_me = ATOMIC_FLAG_INIT;
    };
    class SwitchBoard : public SOS::MemoryView::Notify, public std::array<DMAObjectAsyncSwitch, NUM_IDS>
    {
    public:
        SwitchBoard() : Notify(), std::array<DMAObjectAsyncSwitch, NUM_IDS> {} {}
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
    struct serial_async_tag { };
    template <typename... Objects>
    struct SerialAsyncBus : bus<
        serial_async_tag,
        SOS::MemoryView::SwitchBoard,
        bus_traits<SOS::MemoryView::Bus>::cables_type,
        bus_traits<SOS::MemoryView::Bus>::const_cables_type>
    {
        signal_type signal;
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
    template <typename S, typename OtherBus>
    class SerialDoublePassthruAsyncController : public Controller<S>, public Loop {
    public:
        SerialDoublePassthruAsyncController(typename S::bus_type& passThru, OtherBus& other)
        : Controller<S>()
        , Loop()
        , _foreign(passThru)
        , _child(_foreign, other)
        {
        }

    protected:
        typename S::bus_type& _foreign;

    private:
        S _child;
    };
    template <typename S, typename... Objects>
    class SequentialResolver : public SerialDoublePassthruAsyncController<S, SOS::MemoryView::SerialAsyncBus<Objects...>> {
    public:
        using bus_type = SOS::MemoryView::SerialAsyncBus<Objects...>;
        SequentialResolver(SOS::MemoryView::ComBus<COM_BUFFER>& passThru) // constexpr
            : SerialDoublePassthruAsyncController<S, SOS::MemoryView::SerialAsyncBus<Objects...>>(passThru, _sBus)
        {
        }
        ~SequentialResolver() {
            std::cout << typeid(*this).name() << "ObjectReadsCanceled" << objectReadsCanceled << std::endl;
            std::cout << typeid(*this).name() << "ObjectWritesCanceled" << objectWritesCanceled << std::endl;
        }
        void resolve(std::size_t id) {
            if (!this->_sBus.signal[id].read_ack.test_and_set()) {
                if (this->_sBus.signal[id].read_fault.test_and_set()) {
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
                    read[id].result = true;
                    read[id].ready.clear();
                } else
                {
                    SFA::util::runtime_error(SFA::util::error_code::ServiceInterruptedByComShutdown, __FILE__, __func__, typeid(*this).name());
                    objectReadsCanceled++;
                    read[id].result = false;
                    read[id].ready.clear();
                }
            }
            if (!this->_sBus.signal[id].write_ack.test_and_set()) {
                if (this->_sBus.signal[id].write_fault.test_and_set()){
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
        bus_type _sBus {};

    //private:
        //std::array<std::array<unsigned char, MAX_OBJ_SIZE>, NUM_IDS> doubleBuffer{};
    private:
        std::size_t objectReadsCanceled = 0;
        std::size_t objectWritesCanceled = 0;
    };
}
}