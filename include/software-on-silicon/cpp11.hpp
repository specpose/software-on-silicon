namespace SOS {
namespace Protocol {
    extern "C" {
    struct DescriptorHelper {
        // no user-provided, inherited, or explicit constructors
        // no private or protected direct non-static data members
        unsigned char size() { return count; }
        const DMADescriptor& operator[](const unsigned char i) const { return arr[i]; };
        DMADescriptor& operator[](const unsigned char i) { return arr[i]; };
        struct DMADescriptor arr[NUM_IDS];
        unsigned char count;// = 0;
    };
    }
}
}