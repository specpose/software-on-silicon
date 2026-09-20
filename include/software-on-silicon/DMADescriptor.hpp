namespace SOS {
#define LOWER_STATES 1
#define UPPER_STATES 5
#define NUM_IDS 64 - UPPER_STATES - LOWER_STATES
#define NUM_SIGNALBITS 2
#define MAX_OBJ_SIZE 252 // 8bit: max, 252%3==0
namespace Protocol {
    extern "C" {
    struct DMADescriptor {
        //void method(){};
        void* obj;// = nullptr;
        unsigned long obj_size;// = 0;
        //bool readLock;// = false;
        //bool transfer;// = false;
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