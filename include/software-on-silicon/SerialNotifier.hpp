namespace SOS {
namespace MemoryView {
    class DMAObjectShake : private SOS::MemoryView::HandShake, private std::array<std::atomic_flag,4> {
    public:
        DMAObjectShake() : std::array<std::atomic_flag,4>{} {
            std::get<0>(*this).test_and_set();
            std::get<1>(*this).test_and_set();
            std::get<2>(*this).test_and_set();
            std::get<3>(*this).test_and_set();
        }
        auto& getReadUpdatedRef(){return std::get<0>(*this);}
        auto& getReadAcknowledgeRef(){return std::get<1>(*this);}
        auto& getWriteUpdatedRef(){return std::get<2>(*this);}
        auto& getWriteAcknowledgeRef(){return std::get<3>(*this);}
        auto& getSyncUpdatedRef(){ return updated; }
        auto& getSyncAcknowledgeRef(){ return acknowledge; }
    };
    struct DestinationAndOrigin : private SOS::MemoryView::TaskCable<std::size_t, 3> {
        DestinationAndOrigin()
        : SOS::MemoryView::TaskCable<std::size_t, 3> { 0, 0, 0 }
        {
        }
        auto& getReceiveNotificationRef() { return std::get<0>(*this); }
        auto& getSendNotificationRef() { return std::get<1>(*this); }
        auto& getSyncNotificationRef() { return std::get<2>(*this); }
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
        auto& receiveNotificationId() { return std::get<0>(cables).getReceiveNotificationRef(); }
        auto& sendNotificationId() { return std::get<0>(cables).getSendNotificationRef(); }
        auto& syncNotificationId() { return std::get<0>(cables).getSyncNotificationRef(); }
    };
}
namespace Protocol {
    enum state : unsigned char {
        idle = 0x00,
        poweron = 0x01,
        sighup = 0x02,
        shutdown = 0x03
    };
    static const unsigned char NUM_STATES = 4;
    static const unsigned char NUM_IDS = 64 - NUM_STATES;
    static const unsigned int NUM_SIGNALBITS = 2;
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
        SerialProcessing(bus_type& bus) : _datasignals(bus), SOS::Behavior::SerialDummy<>(bus.signal) {}
        void event_loop()
        {
            if (!_intrinsic.getWriteAcknowledgeRef().test_and_set()) {
                _intrinsic.getWriteUpdatedRef().test_and_set();//not used
                const auto id = _datasignals.sendNotificationId().load();
                write[id] = false;
                write_notify_hook(id);
            }
            if (!_intrinsic.getReadAcknowledgeRef().test_and_set()) {
                _intrinsic.getReadUpdatedRef().test_and_set();//not used
                const auto id = _datasignals.receiveNotificationId().load();
                read[id] = false;
                read_notify_hook(id);
            }
            std::this_thread::yield();
        }

    protected:
        virtual void write_notify_hook(std::size_t object_id) = 0;
        virtual void read_notify_hook(std::size_t object_id) = 0;
        std::bitset<SOS::Protocol::NUM_IDS> read{};
        std::bitset<SOS::Protocol::NUM_IDS> write{};
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
            (assign(obj_ref), ...);
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
}
namespace MemoryView {

    template <typename... Objects>
    struct SerialProcessNotifier : public BusDMAShaker {
        SerialProcessNotifier()
        {
            std::apply(descriptors, objects); // ALWAYS: Initialize Descriptors in Constructor
        }
        std::tuple<Objects...> objects {};
        SOS::Protocol::DescriptorHelper<std::tuple_size<std::tuple<Objects...>>::value> descriptors {};
    };
}
}