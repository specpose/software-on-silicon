#include <chrono>
template<typename... Objects> void dump_objects(std::tuple<Objects...>& objects,
SOS::Protocol::DescriptorHelper<std::tuple_size<std::tuple<Objects...>>::value>& descriptors,
std::chrono::time_point<std::chrono::high_resolution_clock> boot_time,
std::chrono::time_point<std::chrono::high_resolution_clock> kill_time) {
    for (int i=0;i<sizeof...(Objects);i++){
        double t = std::chrono::duration_cast<std::chrono::nanoseconds>(kill_time - boot_time).count();
        std::cout<<"Object ID: "<<static_cast<unsigned long>(descriptors[i].id)
        <<"; RX: "<<descriptors[i].rx_counter<<" => "<<descriptors[i].rx_counter/(t/std::nano::den)<<" Symbols/s"
        <<"; TX: "<<descriptors[i].tx_counter<<" => "<<descriptors[i].tx_counter/(t/std::nano::den)<<" Symbols/s"<<std::endl;
        for (std::size_t j=0;j<descriptors[i].obj_size;j++){
            printf("%X", reinterpret_cast<unsigned char*>(descriptors[i].obj)[j] );
        }
        std::cout<<std::endl;
    }
};