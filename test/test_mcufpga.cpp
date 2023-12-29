#include "MCUFPGA.cpp"

int main () {
    SOS::MemoryView::ComBus<COM_BUFFER> bus{std::begin(fpga_to_mcu_buffer),std::end(fpga_to_mcu_buffer),std::begin(mcu_to_fpga_buffer),std::end(mcu_to_fpga_buffer)};//SIMULATION: Handshake = uart_getc
    auto host= new MCU(bus);
    auto client= new FPGA(bus);
    const auto start = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < 1) {
        std::this_thread::yield();
    }
    //host._child.stop();
    //host.stop();
    delete client;
    delete host;
}
