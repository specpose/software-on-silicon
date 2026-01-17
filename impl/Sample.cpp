namespace SOS {
    namespace MemoryView {
        template<typename channel_t, std::size_t num_channels> struct sample {
            std::array<channel_t,num_channels> channels{{0}};
        };
    }
}