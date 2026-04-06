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
            for (std::size_t i = 0; i < count; i++) {
                std::cout << "DMAObject " << i << " ptr: " << (*this)[i].obj << " size: " << (*this)[i].obj_size << std::endl;
            }
        }
        std::size_t size() { return count; }

    private:
        template <typename First>
        void assign(First& obj_ref)
        {
            (*this)[count] = { static_cast<unsigned char>(count), reinterpret_cast<void*>(&obj_ref), sizeof(obj_ref), false, false, false };
            count++;
        }
        template <typename First, typename... Others>
        void assign(First& obj_ref, Others&... objects)
        {
            (*this)[count] = { static_cast<unsigned char>(count), reinterpret_cast<void*>(&obj_ref), sizeof(obj_ref), false, false, false };
            count++;
            assign(objects...);
        }
        std::size_t count = 0;
    };
}
}