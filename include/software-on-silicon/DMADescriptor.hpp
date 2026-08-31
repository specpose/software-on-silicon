namespace SOS {
#define LOWER_STATES 1
#define UPPER_STATES 5
#define NUM_IDS 64 - UPPER_STATES - LOWER_STATES
#define NUM_SIGNALBITS 2
namespace Protocol {
    extern "C" {
    struct DMADescriptor {
        void method(){};
        unsigned char id;// = NUM_IDS;
        void* obj;// = nullptr;
        unsigned long obj_size;// = 0;
        volatile bool readLock;// = false;
        volatile bool unsynced;// = false;
        bool transfer;// = false;
        std::array<volatile bool, 2> read_status; // word not clock?
        std::array<volatile bool, 2> write_status; // word not clock?
        volatile bool sync_me;
    };
    }
    // template <typename... Objects>
    // struct ObjectHelper : public std::tuple<Objects&...> {
    //     ObjectHelper(Objects&&... obj_refs) : std::tuple<Objects&...>{std::forward(obj_refs...)} {}
    // };
    // template<typename ArithmeticType, std::size_t N> struct Array : public SOS::MemoryView::TaskCable<ArithmeticType, N> {
    //     using SOS::MemoryView::TaskCable<ArithmeticType, N>::TaskCable;
    // };
}
}