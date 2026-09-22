template<typename... Objects> SOS::Protocol::DescriptorHelper cpp11_static_descriptors(std::tuple<Objects...> objects) {
    const unsigned char s = 3;
    /*return { { { reinterpret_cast<void*>(&std::get<0>(objects)), sizeof(std::get<0>(objects)) }, { reinterpret_cast<void*>(&std::get<1>(objects)), sizeof(std::get<1>(objects)) }, { reinterpret_cast<void*>(&std::get<2>(objects)), sizeof(std::get<2>(objects)) } }, s};*/
    SOS::Protocol::DescriptorHelper helper;
    helper.arr[0].obj = reinterpret_cast<void*>(&std::get<0>(objects));
    helper.arr[0].obj_size = sizeof(std::get<0>(objects));
    helper.arr[1].obj = reinterpret_cast<void*>(&std::get<1>(objects));
    helper.arr[1].obj_size = sizeof(std::get<1>(objects));
    helper.arr[2].obj = reinterpret_cast<void*>(&std::get<2>(objects));
    helper.arr[2].obj_size = sizeof(std::get<2>(objects));
    helper.count = 3;
    for (std::size_t i = s; i < NUM_IDS; ++i){
        helper.arr[i].obj = (void*)nullptr;
        helper.arr[i].obj_size = 0;
    }
    return helper;
}
template<typename... Objects> void print_descriptors(SOS::Protocol::DescriptorHelper& descriptors) {
    for (std::size_t i = 0; i < descriptors.size(); i++) {
        std::cout << "DMAObject " << i << " ptr: " << (descriptors)[i].obj << " size: " << (descriptors)[i].obj_size << std::endl;
    }
}