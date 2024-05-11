#include "MCUFPGA.cpp"
COM_BUFFER fpga_in_buffer;
COM_BUFFER fpga_out_buffer;
COM_BUFFER mcu_in_buffer;
COM_BUFFER mcu_out_buffer;

int main () {
    SOS::MemoryView::ComBus<COM_BUFFER> mcubus{std::begin(mcu_in_buffer),std::end(mcu_in_buffer),std::begin(mcu_out_buffer),std::end(mcu_out_buffer)};
    auto host= new MCU(mcubus);//SIMULATION: requires additional thread. => remove thread from MCU
    fpga_out_buffer[0]=SOS::Protocol::idleState().to_ulong();//INIT: FPGA initiates communication with an idle byte
    SOS::MemoryView::ComBus<COM_BUFFER> fpgabus{std::begin(fpga_in_buffer),std::end(fpga_in_buffer),std::begin(fpga_out_buffer),std::end(fpga_out_buffer)};
    fpgabus.signal.getAcknowledgeRef().clear();//INIT: start one-way handshake
    auto client= new FPGA(fpgabus);//SIMULATION: requires additional thread. => remove thread from FPGA
    const auto start = std::chrono::high_resolution_clock::now();
    bool client_request_stop = false;
    bool stop = false;
    while (!stop) {
        //SIMULATION HOST PART
        if (!mcubus.signal.getAcknowledgeRef().test_and_set()){
            //transfer mcu_out_buffer to fpga_in_buffer
            for (std::size_t n=0;n<fpga_in_buffer.size();n++)
                fpga_in_buffer[n]=mcu_out_buffer[n];
            fpgabus.signal.getUpdatedRef().clear();
        }

        //SIMULATION CLIENT PART
        if (!client_request_stop && !(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < 1)){
            client->requestStop();
            client_request_stop = true;
            stop = true;//CUTS THE LINE => no last sync possible?
        }
        if (!fpgabus.signal.getAcknowledgeRef().test_and_set()){
            //transfer fpga_out_buffer to mcu_in_buffer
            for (std::size_t n=0;n<mcu_in_buffer.size();n++)
                mcu_in_buffer[n]=fpga_out_buffer[n];
            mcubus.signal.getUpdatedRef().clear();
        }

        std::this_thread::yield();
    }
    delete client;
    delete host;
}
