namespace SOS {
namespace Protocol {
    struct DescriptorHelper : public std::array<DMADescriptor, NUM_IDS> {
    public:
        using std::array<DMADescriptor, NUM_IDS>::array;
        template <typename... Object>
        void operator()(Object&... objects)
        {
            count = 0;
            (assign(objects), ...); // fold expression: cpp17
        }
        unsigned char& size() { return count; }

    private:
        template <typename First>
        void assign(First& obj_ref)
        {
            (*this)[count] = DMADescriptor{ reinterpret_cast<void*>(&obj_ref), sizeof(obj_ref)};
            count++;
        }
        template <typename First, typename... Others>
        void assign(First& obj_ref, Others&... objects)
        {
            (*this)[count] = DMADescriptor{ reinterpret_cast<void*>(&obj_ref), sizeof(obj_ref)};
            count++;
            assign(objects...);
        }
        unsigned char count = 0;
    };
}
}