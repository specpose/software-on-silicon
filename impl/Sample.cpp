namespace SOS {
namespace MemoryView {
    template <typename channel_t, std::size_t N>
    struct sample {
        using sample_type = channel_t;
        const static std::size_t num_channels = N;
        // AGGREGATE INITIALISATION: no constructors
        channel_t channels[N];
        const channel_t& operator[](const std::size_t i) const { return channels[i]; };
        channel_t& operator[](const std::size_t i) { return channels[i]; };
    };
}
}