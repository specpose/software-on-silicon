namespace SOS {
namespace Protocol {
    extern "C" {
    struct DescriptorHelper {
        // no user-provided, inherited, or explicit constructors
        // no private or protected direct non-static data members
        std::size_t size() { return count; }
        const DMADescriptor& operator[](const std::size_t i) const { return arr[i]; };
        DMADescriptor& operator[](const std::size_t i) { return arr[i]; };
        struct DMADescriptor arr[NUM_IDS];
        std::size_t count = 0;
    };
    }
}
}