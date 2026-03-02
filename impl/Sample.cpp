namespace SOS {
    namespace MemoryView {
        template<typename channel_t, std::size_t N> struct sample {
            using sample_type = channel_t;
            const static std::size_t num_channels = N;
            //AGGREGATE INITIALISATION: no constructors
            std::array<channel_t, N> channels{};
        };
    }
}