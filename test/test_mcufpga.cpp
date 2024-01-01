#include "MCUFPGA.cpp"
COM_BUFFER fpga_in_buffer;
COM_BUFFER fpga_out_buffer;
COM_BUFFER mcu_in_buffer;
COM_BUFFER mcu_out_buffer;

int main () {
    SOS::MemoryView::ComBus<COM_BUFFER> mcubus{std::begin(mcu_in_buffer),std::end(mcu_in_buffer),std::begin(mcu_out_buffer),std::end(mcu_out_buffer)};
    auto host= new MCU(mcubus);
    fpga_out_buffer[0]=SOS::Protocol::idleState().to_ulong();//INIT: FPGA initiates communication with an idle byte
    SOS::MemoryView::ComBus<COM_BUFFER> fpgabus{std::begin(fpga_in_buffer),std::end(fpga_in_buffer),std::begin(fpga_out_buffer),std::end(fpga_out_buffer)};
    fpgabus.signal.getAcknowledgeRef().clear();//INIT: start one-way handshake
    auto client= new FPGA(fpgabus);
    const auto start = std::chrono::high_resolution_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < 1) {//CUTS THE LINE => no last sync possible
        //SIMULATION: requires additional thread. => remove thread from parent Controllers
        //if (termios.read(Ctx,&mcu_in_buffer,1)){//SIMULATION: Enable 2
        //    mcubus.signal.getUpdatedRef().clear();//SIMULATION: Enable 2
        //    while (mcubus.signal.getAcknowledgeRef().test_and_set())
        //        std::this_thread::yield();
        if (!mcubus.signal.getAcknowledgeRef().test_and_set()){//SIMULATION: Disable 2
            //transfer mcu_out_buffer to fpga_in_buffer
            for (std::size_t n=0;n<fpga_in_buffer.size();n++)
                fpga_in_buffer[n]=mcu_out_buffer[n];
            //SIMULATION: termios write(Ctx,&mcu_out_buffer,1)
            fpgabus.signal.getUpdatedRef().clear();//SIMULATION: Disable 2
        }
        //}
        //if (termios.read(Ctx,&fpga_in_buffer,1)){//SIMULATION: Enable 1
        //    fpgabus.signal.getUpdatedRef().clear();//SIMULATION: Enable 1
        //    while (fpgabus.signal.getAcknowledgeRef().test_and_set())
        //        std::this_thread::yield();
        if (!fpgabus.signal.getAcknowledgeRef().test_and_set()){//SIMULATION: Disable 1
            //transfer fpga_out_buffer to mcu_in_buffer
            for (std::size_t n=0;n<mcu_in_buffer.size();n++)
                mcu_in_buffer[n]=fpga_out_buffer[n];
            //SIMULATION: termios write(Ctx,&fpga_out_buffer,1)
            mcubus.signal.getUpdatedRef().clear();//SIMULATION: Disable 1
        }
        //}
        std::this_thread::yield();
    }
    //host._child.stop();
    //host.stop();
    delete client;
    std::this_thread::sleep_for(std::chrono::milliseconds{500});
    delete host;
}
