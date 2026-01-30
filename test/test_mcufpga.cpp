#include "MCUFPGA.cpp"
COM_BUFFER fpga_in_buffer;
COM_BUFFER fpga_out_buffer;
bool firstrun = true;
COM_BUFFER mcu_in_buffer;
COM_BUFFER mcu_out_buffer;

void client_funct(COM_BUFFER& fpga_in_buffer, COM_BUFFER& mcu_in_buffer, COM_BUFFER& fpga_out_buffer, COM_BUFFER& mcu_out_buffer,
    SOS::MemoryView::ComBus<COM_BUFFER>& mcubus,SOS::MemoryView::ComBus<COM_BUFFER>& fpgabus, bool stopped=false){
    if (firstrun){//ALWAYS: expect first byte not to be read and poweronstate is being written
        fpgabus.signal.getUpdatedRef().clear();//ALWAYS: flip the first handshake
        firstrun = false;
    }
    if (!fpgabus.signal.getAcknowledgeRef().test_and_set()){
        //transfer fpga_out_buffer to mcu_in_buffer
        for (std::size_t n=0;n<mcu_in_buffer.size();n++){
            mcu_in_buffer[n]=fpga_out_buffer[n];
        }
        mcubus.signal.getUpdatedRef().clear();
    }
    std::this_thread::yield();
};
void host_funct(COM_BUFFER& fpga_in_buffer, COM_BUFFER& mcu_in_buffer, COM_BUFFER& fpga_out_buffer, COM_BUFFER& mcu_out_buffer,
    SOS::MemoryView::ComBus<COM_BUFFER>& mcubus,SOS::MemoryView::ComBus<COM_BUFFER>& fpgabus, bool& nomoresignal, std::chrono::time_point<std::chrono::high_resolution_clock>& nomoresignal_time, bool stopped=false){
    bool oldsignal = nomoresignal;
    if (!mcubus.signal.getAcknowledgeRef().test_and_set()){
        nomoresignal = false;
        //transfer mcu_out_buffer to fpga_in_buffer
        for (std::size_t n=0;n<fpga_in_buffer.size();n++){
            fpga_in_buffer[n]=mcu_out_buffer[n];
        }
        fpgabus.signal.getUpdatedRef().clear();
    } else {
        nomoresignal = true;
        if (nomoresignal != oldsignal)
            nomoresignal_time = std::chrono::high_resolution_clock::now();
    }
    std::this_thread::yield();
};

int main () {
    SOS::MemoryView::ComBus<COM_BUFFER> mcubus{std::begin(mcu_in_buffer),std::end(mcu_in_buffer),std::begin(mcu_out_buffer),std::end(mcu_out_buffer)};
    auto host= new MCU(mcubus);//SIMULATION: requires additional thread. => remove thread from MCU
    std::thread host_stopper;
    bool host_request_stop = false;
    bool host_delete = false;
    SOS::MemoryView::ComBus<COM_BUFFER> fpgabus{std::begin(fpga_in_buffer),std::end(fpga_in_buffer),std::begin(fpga_out_buffer),std::end(fpga_out_buffer)};
    using namespace std::chrono_literals;
    //std::this_thread::sleep_for(300ms);//SIMULATION: no way to check if thread is running without interfering with handshake
    auto client= new FPGA(fpgabus);//SIMULATION: requires additional thread. => remove thread from FPGA
    std::thread client_stopper;
    bool client_request_stop = false;
    bool client_delete = false;
    bool stop = false;
    bool nomoresignal = false;
    const auto start = std::chrono::high_resolution_clock::now();
    auto nomoresignal_time = start;
    bool handshake_stop = false;
    auto handshake = std::thread([&](){
        while (!handshake_stop) {
            host_funct(fpga_in_buffer, mcu_in_buffer, fpga_out_buffer, mcu_out_buffer, mcubus, fpgabus, nomoresignal, nomoresignal_time);
            client_funct(fpga_in_buffer, mcu_in_buffer, fpga_out_buffer, mcu_out_buffer, mcubus, fpgabus);
            std::this_thread::yield();
        } });
    while (!stop) {
        if (!host && !client)
            stop = true;

        //HOST THREAD
        if (nomoresignal && !(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - nomoresignal_time).count() < 2)){
            if (!host_request_stop){
                mcubus.signal.getAuxUpdatedRef().clear();
                host_request_stop = true;
            } else {
                if (host && !host_delete)
                    if (host->isStopped()){
                        host_stopper = std::move(std::thread([](MCU** process){ delete *process; *process = nullptr; },&host));
                        //delete host;
                        host_delete = true;
                    }
            }
        }

        //CLIENT THREAD
        if (!(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < 1)){
            if (!client_request_stop){
                fpgabus.signal.getAuxUpdatedRef().clear();
                client_request_stop = true;
	        } else {
                if (client && !client_delete)
                    if (client->isStopped()){
                        client_stopper = std::move(std::thread([](FPGA** process){ delete *process; *process = nullptr; },&client));
                        //delete client;
                        client_delete = true;
                    }
            }
        }

        std::this_thread::yield();
    }
    client_stopper.join();
    host_stopper.join();
    handshake_stop = true;
    handshake.join();
}
