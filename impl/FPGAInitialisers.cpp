template<typename... Objects> SOS::Protocol::DescriptorHelper cpp11_static_descriptors(std::tuple<Objects...> objects) {
    const std::size_t s = 3;
    return { { { reinterpret_cast<void*>(&std::get<0>(objects)), sizeof(std::get<0>(objects)) },
    { reinterpret_cast<void*>(&std::get<1>(objects)), sizeof(std::get<1>(objects)) },
    { reinterpret_cast<void*>(&std::get<2>(objects)), sizeof(std::get<2>(objects)) } },
    s};
}
template<typename... Objects> void print_descriptors(SOS::Protocol::DescriptorHelper& descriptors) {
    for (std::size_t i = 0; i < descriptors.size(); i++) {
        std::cout << "DMAObject " << i << " ptr: " << (descriptors)[i].obj << " size: " << (descriptors)[i].obj_size << std::endl;
    }
}