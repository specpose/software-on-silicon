#include <chrono>
template<typename... Objects> void dump_objects(
    std::tuple<Objects...>& objects,
    std::array<unsigned long, SOS::Protocol::NUM_IDS>& rx_counter,
    std::array<unsigned long, SOS::Protocol::NUM_IDS>& tx_counter,
    //SOS::Protocol::DescriptorHelper<std::tuple_size<std::tuple<Objects...>>::value>& descriptors,
    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time,
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time) {
    for (int i=0;i<sizeof...(Objects);i++){
        double t = std::chrono::duration_cast<std::chrono::nanoseconds>(kill_time - boot_time).count();
        std::cout
        //<<"Object ID: "<<static_cast<unsigned long>(descriptors[i].id)
        <<"; RX: "<<rx_counter[i]<<" => "<<rx_counter[i]/(t/std::nano::den)<<" Symbols/s"
        <<"; TX: "<<tx_counter[i]<<" => "<<tx_counter[i]/(t/std::nano::den)<<" Symbols/s"<<std::endl;
        //for (std::size_t j=0;j<descriptors[i].obj_size;j++){
        //    printf("%X", reinterpret_cast<unsigned char*>(descriptors[i].obj)[j] );
        //}
        std::cout<<std::endl;
    }
};