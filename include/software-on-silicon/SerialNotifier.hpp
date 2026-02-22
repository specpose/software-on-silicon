namespace SOS {
namespace MemoryView {
    class DMAObjectShake : private SOS::MemoryView::HandShake, private std::array<std::atomic_flag,6> {
    public:
        DMAObjectShake() : std::array<std::atomic_flag,6>{} {
            std::get<0>(*this).test_and_set();
            std::get<1>(*this).test_and_set();
            std::get<2>(*this).test_and_set();
            std::get<3>(*this).test_and_set();
            std::get<4>(*this).test_and_set();
            std::get<5>(*this).test_and_set();
        }
        auto& getReadUpdatedRef(){return std::get<0>(*this);}
        auto& getReadAcknowledgeRef(){return std::get<1>(*this);}
        auto& getWriteUpdatedRef(){return std::get<2>(*this);}
        auto& getWriteAcknowledgeRef(){return std::get<3>(*this);}
        auto& getSyncStartUpdatedRef(){return std::get<4>(*this);}
        auto& getSyncStartAcknowledgeRef(){return std::get<5>(*this);}
        auto& getSyncStopUpdatedRef(){return updated;}
        auto& getSyncStopAcknowledgeRef(){return acknowledge;}
    };
    struct DestinationAndOrigin : public SOS::MemoryView::TaskCable<std::size_t, 4> {
        DestinationAndOrigin()
        : SOS::MemoryView::TaskCable<std::size_t, 4> { 0, 0, 0, 0 } {}
    };
    struct bus_dma_shaker_tag{};
    struct BusDMAShaker : bus <
    bus_dma_shaker_tag,
    SOS::MemoryView::DMAObjectShake,
    std::tuple<DestinationAndOrigin>,
    bus_traits<Bus>::const_cables_type
    >{
        signal_type signal;
        cables_type cables;
        auto& receiveNotificationId() { return std::get<0>(std::get<0>(cables)); }
        auto& sendNotificationId() { return std::get<1>(std::get<0>(cables)); }
        auto& syncStopId() { return std::get<2>(std::get<0>(cables)); }
        auto& syncStartId() { return std::get<3>(std::get<0>(cables)); }
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
    static const unsigned char LOWER_STATES = 1;
    static const unsigned char UPPER_STATES = 5;
    static const unsigned char NUM_IDS = 64 - UPPER_STATES - LOWER_STATES;
    static const unsigned int NUM_SIGNALBITS = 2;
}
namespace MemoryView {
    template<unsigned char N>
    struct Futures : public std::array<std::future<bool>,N> {
        Futures() : std::array<std::future<bool>,N>{} {}
    };
    //template<unsigned char N>
    //struct Promises : public std::array<std::promise<bool>,N> {
    //    //Promises(Objects&&... obj_refs) : std::tuple<std::promise<Objects&>...>{std::forward(obj_refs...)} {}
    //    Promises() : std::array<std::promise<bool>,N>{} {
    //        for (std::size_t i = 0; i < this->size(); ++i){
    //            (*this)[i].set_value(true);
    //        }
    //    }
    //};
}
namespace Behavior {
    class SerialSubController : public SubController {
    public:
        using bus_type = SOS::MemoryView::BusDMAShaker;
        constexpr SerialSubController(typename bus_type::signal_type& signal) : SubController(), _intrinsic(signal) {}
    protected:
        bus_type::signal_type& _intrinsic;
    };
    template<typename... Others> class SerialDummy : public Loop, protected SerialSubController {
    public:
        SerialDummy(typename bus_type::signal_type& signal, Others&... args) : Loop(), SerialSubController(signal) {}
    };
    class SerialProcessing : public SOS::Behavior::SerialDummy<> {
    public:
        using bus_type = SOS::MemoryView::BusDMAShaker;
        SerialProcessing(bus_type& bus) : _datasignals(bus), SOS::Behavior::SerialDummy<>(bus.signal) {
            for (std::size_t i = 0; i < SOS::Protocol::NUM_IDS; ++i){
                write_fault[i].test_and_set();
                write_ack[i].test_and_set();
                write_status[i] = std::async(std::launch::deferred,[]()->bool{ return true; });
            }
            _intrinsic.getSyncStartUpdatedRef().clear();
            readOrWrite = true; // one sync is enough to trigger a read or write hook
            _intrinsic.getSyncStopUpdatedRef().clear();
            _intrinsic.getWriteUpdatedRef().clear();
            _intrinsic.getReadUpdatedRef().clear();
        }
        void event_loop()
        {
            if (!_intrinsic.getSyncStopAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.syncStopId().load();
                if (sync[id])
                    if (id==1 || id==2){
                        std::cout << typeid(*this).name() << ": write of object id " << id << " canceled" << std::endl;
                        write_fault[id].clear();
                    }
                sync[id] = false;
                sync_backup[id] = false;
                _intrinsic.getSyncStopUpdatedRef().clear();
            }
            if (!_intrinsic.getReadAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.receiveNotificationId().load();
                read[id] = false;
                _intrinsic.getReadUpdatedRef().clear();
                read_notify_hook(id);
                readOrWrite = true;
            }
            process_hook();
            if (!_intrinsic.getWriteAcknowledgeRef().test_and_set()) {
                const auto id = _datasignals.sendNotificationId().load();
                write[id] = false;
                if (id==1 || id==2){
                    std::cout << typeid(*this).name() << ": write of object id " << id << " succeeded" << std::endl;
                    write_ack[id].clear();
                }
                _intrinsic.getWriteUpdatedRef().clear();
                write_notify_hook(id);
                readOrWrite = true;
            }
            if (readOrWrite) {
                for (std::size_t i=0; i<SOS::Protocol::NUM_IDS; i++) {
                    if (sync[i] && !sync_backup[i]){
                        if (!_intrinsic.getSyncStartUpdatedRef().test_and_set()){
                            sync_backup[i] = true;
                            _datasignals.syncStartId().store(i);
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
        virtual void write_notify_hook(std::size_t object_id) = 0;
        virtual void read_notify_hook(std::size_t object_id) = 0;
        virtual void process_hook() = 0;
        std::bitset<SOS::Protocol::NUM_IDS> read{};
        std::bitset<SOS::Protocol::NUM_IDS> write{};
        std::array<std::atomic_flag,SOS::Protocol::NUM_IDS> write_fault{};
        std::array<std::atomic_flag,SOS::Protocol::NUM_IDS> write_ack{};
        SOS::MemoryView::Futures<SOS::Protocol::NUM_IDS> write_status{};
        std::bitset<SOS::Protocol::NUM_IDS> sync{};
        std::bitset<SOS::Protocol::NUM_IDS> sync_backup{};
    private:
        bus_type& _datasignals;
    };
}
namespace Protocol {
    struct DMADescriptor {
        DMADescriptor() { } // DANGER
        DMADescriptor(unsigned char id, void* obj, std::size_t obj_size)
            : id(id)
            , obj(obj)
            , obj_size(obj_size)
        {
            if (obj_size % 3 != 0)
                SFA::util::logic_error(SFA::util::error_code::InvalidDMAObjectSize, __FILE__, __func__, typeid(*this).name());
        }
        unsigned char id = NUM_IDS;
        void* obj = nullptr;
        std::size_t obj_size = 0;
        volatile bool readLock = false; // SerialProcessing thread
        volatile bool unsynced = false; // SerialProcessing thread
        bool transfer = false;
    };
    template <unsigned int N>
    struct DescriptorHelper : public std::array<DMADescriptor, N> {
    public:
        using std::array<DMADescriptor, N>::array;
        template <typename... T>
        void operator()(T&... obj_ref)
        {
            (assign(obj_ref), ...);//fold expression: cpp17
        }

    private:
        template <typename T>
        void assign(T& obj_ref)
        {
            (*this)[count] = DMADescriptor(static_cast<unsigned char>(count), reinterpret_cast<void*>(&obj_ref), sizeof(obj_ref));
            count++;
        }
        std::size_t count = 0;
    };
    //template <typename... Objects>
    //struct ObjectHelper : public std::tuple<Objects&...> {
    //    ObjectHelper(Objects&&... obj_refs) : std::tuple<Objects&...>{std::forward(obj_refs...)} {}
    //};
    //template<typename ArithmeticType, std::size_t N> struct Array : public SOS::MemoryView::TaskCable<ArithmeticType, N> {
    //    using SOS::MemoryView::TaskCable<ArithmeticType, N>::TaskCable;
    //};
    template<typename ArithmeticType> struct Size : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
        Size(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
    };
    template <typename Object, std::size_t Sizeof = sizeof(Object), typename ArithmeticType = typename std::enable_if<Sizeof%3==0&&Sizeof%12!=0&&Sizeof%24!=0&&Sizeof%12288!=0, unsigned char>::type>
    class CharBusGenerator : public SOS::MemoryView::BusShaker {
    public:
        CharBusGenerator() = delete;
        CharBusGenerator(Object& obj) :
        SOS::MemoryView::BusShaker{},
        const_cables{ Size<ArithmeticType*>(reinterpret_cast<ArithmeticType*>(&obj), reinterpret_cast<ArithmeticType*>(&obj)+Sizeof) } {}
        using const_cables_type = std::tuple< Size<ArithmeticType*> >;
        const_cables_type const_cables;
    };
}
namespace MemoryView {

    template <typename... Objects>
    struct SerialProcessNotifier : public BusDMAShaker {
        std::tuple<Objects...> objects {};
    };
}
namespace Protocol {
    bool write_status(std::atomic_flag& fault, std::atomic_flag& ack) {
        bool exit = false;
        bool result = false;
        while( !exit ) {
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
}