#include "MCUFPGA.cpp"
COM_BUFFER fpga_in_buffer;
COM_BUFFER fpga_out_buffer;
COM_BUFFER mcu_in_buffer;
COM_BUFFER mcu_out_buffer;

void client_funct(COM_BUFFER& fpga_in_buffer, COM_BUFFER& mcu_in_buffer, COM_BUFFER& fpga_out_buffer, COM_BUFFER& mcu_out_buffer,
    SOS::MemoryView::ComBus<COM_BUFFER>& mcubus,SOS::MemoryView::ComBus<COM_BUFFER>& fpgabus){
    if (!fpgabus.signal.getAcknowledgeRef().test_and_set()){
        //transfer fpga_out_buffer to mcu_in_buffer
        for (std::size_t n=0;n<mcu_in_buffer.size();n++)
            mcu_in_buffer[n]=fpga_out_buffer[n];
        mcubus.signal.getUpdatedRef().clear();
    }
    std::this_thread::yield();
};
void host_funct(COM_BUFFER& fpga_in_buffer, COM_BUFFER& mcu_in_buffer, COM_BUFFER& fpga_out_buffer, COM_BUFFER& mcu_out_buffer,
    SOS::MemoryView::ComBus<COM_BUFFER>& mcubus,SOS::MemoryView::ComBus<COM_BUFFER>& fpgabus){
    if (!mcubus.signal.getAcknowledgeRef().test_and_set()){
        //transfer mcu_out_buffer to fpga_in_buffer
        for (std::size_t n=0;n<fpga_in_buffer.size();n++)
            fpga_in_buffer[n]=mcu_out_buffer[n];
        fpgabus.signal.getUpdatedRef().clear();
    }
    std::this_thread::yield();
};

int main () {
    SOS::MemoryView::ComBus<COM_BUFFER> mcubus{std::begin(mcu_in_buffer),std::end(mcu_in_buffer),std::begin(mcu_out_buffer),std::end(mcu_out_buffer)};
    auto host= new MCU(mcubus);//SIMULATION: requires additional thread. => remove thread from MCU
    bool host_request_stop = false;
    SOS::MemoryView::ComBus<COM_BUFFER> fpgabus{std::begin(fpga_in_buffer),std::end(fpga_in_buffer),std::begin(fpga_out_buffer),std::end(fpga_out_buffer)};
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(300ms);//SIMULATION: no way to check if thread is running without interfering with handshake
    auto client= new FPGA(fpgabus);//SIMULATION: requires additional thread. => remove thread from FPGA
    bool client_request_stop = false;
    bool stop = false;
    const auto start = std::chrono::high_resolution_clock::now();
    while (!stop) {
        if (host->isStopped() && client->isStopped())
            stop = true;

        //HOST THREAD
        if (!(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < 2)){
            if (!host_request_stop){
                host->requestStop();
                host_request_stop = true;
            }
        }
        if (!host->isStopped())
            host_funct(fpga_in_buffer, mcu_in_buffer, fpga_out_buffer, mcu_out_buffer, mcubus, fpgabus);

        //CLIENT THREAD
        if (!(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < 1)){
            if (!client_request_stop){
	            client->requestStop();
                client_request_stop = true;
	        }
        }
        if (!client->isStopped())
            client_funct(fpga_in_buffer, mcu_in_buffer, fpga_out_buffer, mcu_out_buffer, mcubus, fpgabus);

        std::this_thread::yield();
    }
    delete client;
    delete host;
}
