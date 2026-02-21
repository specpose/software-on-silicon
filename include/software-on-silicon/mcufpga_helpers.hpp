#include <chrono>
template<typename... Objects> void dump_objects(
    std::tuple<Objects...>& objects,
    std::array<unsigned long, SOS::Protocol::NUM_IDS>& rx_counter,
    std::array<unsigned long, SOS::Protocol::NUM_IDS>& tx_counter,
    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time,
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time) {
    for (int i=0;i<sizeof...(Objects);i++){
        double t = std::chrono::duration_cast<std::chrono::nanoseconds>(kill_time - boot_time).count();
        std::cout
        <<"; RX: "<<rx_counter[i]<<" => "<<rx_counter[i]/(t/std::nano::den)<<" Symbols/s"
        <<"; TX: "<<tx_counter[i]<<" => "<<tx_counter[i]/(t/std::nano::den)<<" Symbols/s"<<std::endl;
        std::cout<<std::endl;
    }
};
template<typename Object> void dump(std::future<bool>&& fut, Object& obj) {// need the type of object so select the correct dump, id is not enough
    if (fut.get())
        std::cout << obj << std::endl;
}
//void dump_DMA(std::future<bool>&& fut, unsigned char id) {
//    if (fut.get())
//        std::cout << "Callback object id " << static_cast<unsigned int>(id) << std::endl;
//};
bool is_promise_ready(std::future<bool>& fut) {
    std::future_status s;
    if (fut.valid())
        s = fut.wait_for(std::chrono::seconds::zero());
    if (s != std::future_status::ready)
        return false;
    return true;
}