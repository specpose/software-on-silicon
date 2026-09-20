#include "MCUFPGA.cpp"
COM_BUFFER fpga_in_buffer;
COM_BUFFER fpga_out_buffer;
bool firstrun = true;
static std::atomic_flag delete_fpga = ATOMIC_FLAG_INIT;
COM_BUFFER mcu_in_buffer;
COM_BUFFER mcu_out_buffer;
static std::atomic_flag delete_mcu = ATOMIC_FLAG_INIT;

#include <unistd.h>
#include <signal.h>
void client_funct(COM_BUFFER& fpga_in_buffer, COM_BUFFER& mcu_in_buffer, COM_BUFFER& fpga_out_buffer, COM_BUFFER& mcu_out_buffer,
    SOS::MemoryView::ComBus<COM_BUFFER>& mcubus, SOS::MemoryView::ComBus<COM_BUFFER>& fpgabus)
{
    if (firstrun) { // ALWAYS: expect first byte not to be read and poweronstate is being written
        fpgabus.signal.getUpdatedRef().clear(); // ALWAYS: flip the first handshake
        firstrun = false;
    }
    if (!fpgabus.signal.getAcknowledgeRef().test_and_set()) {
        // transfer fpga_out_buffer to mcu_in_buffer
        for (std::size_t n = 0; n < mcu_in_buffer.size(); n++) {
            mcu_in_buffer[n] = fpga_out_buffer[n];
        }
        mcubus.signal.getUpdatedRef().clear();
    }
    std::this_thread::yield();
};
void host_funct(COM_BUFFER& fpga_in_buffer, COM_BUFFER& mcu_in_buffer, COM_BUFFER& fpga_out_buffer, COM_BUFFER& mcu_out_buffer,
    SOS::MemoryView::ComBus<COM_BUFFER>& mcubus, SOS::MemoryView::ComBus<COM_BUFFER>& fpgabus)
{
    if (!mcubus.signal.getAcknowledgeRef().test_and_set()) {
        // transfer mcu_out_buffer to fpga_in_buffer
        for (std::size_t n = 0; n < fpga_in_buffer.size(); n++) {
            fpga_in_buffer[n] = mcu_out_buffer[n];
        }
        fpgabus.signal.getUpdatedRef().clear();
    }
    std::this_thread::yield();
};

void usr1_handler(int signum, siginfo_t* info, void* extra)
{
    std::cout << signum << ": thread id " << getpid() << std::endl;
    delete_fpga.clear();
}
void usr2_handler(int signum, siginfo_t* info, void* extra)
{
    std::cout << signum << ": thread id " << getpid() << std::endl;
    delete_mcu.clear();
}

int main()
{
    const char* pidPath = "./test_MCUFPGA.pid";
    FILE* pidFile = fopen(pidPath, "w");
    fprintf(pidFile, "%ld", (long)getpid());
    fclose(pidFile);
    SOS::MemoryView::BusSequentialShaker uart2_mcu {};
    SOS::MemoryView::ComBus<COM_BUFFER> mcubus { std::begin(mcu_in_buffer), std::end(mcu_in_buffer), std::begin(mcu_out_buffer), std::end(mcu_out_buffer) };
    auto host = new MCUSimpleDummy(uart2_mcu, mcubus); // SIMULATION: requires additional thread. => remove thread from MCU
    struct sigaction usr2 = { 0 };
    usr2.sa_sigaction = &usr2_handler;
    sigemptyset(&usr2.sa_mask);
    usr2.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR2, &usr2, NULL);
    bool host_delete = false;
    delete_mcu.test_and_set();
    SOS::MemoryView::BusSequentialShaker uart2_fpga {};
    SOS::MemoryView::ComBus<COM_BUFFER> fpgabus { std::begin(fpga_in_buffer), std::end(fpga_in_buffer), std::begin(fpga_out_buffer), std::end(fpga_out_buffer) };
    auto client = new FPGASimpleDummy(uart2_fpga, fpgabus); // SIMULATION: requires additional thread. => remove thread from FPGA
    struct sigaction usr1 = { 0 };
    usr1.sa_sigaction = &usr1_handler;
    sigemptyset(&usr1.sa_mask);
    usr1.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &usr1, NULL);
    bool client_delete = false;
    delete_fpga.test_and_set();
    bool handshake_stop = false;
    auto handshake = std::thread([&]() {
        while (!handshake_stop) {
            host_funct(fpga_in_buffer, mcu_in_buffer, fpga_out_buffer, mcu_out_buffer, mcubus, fpgabus);
            client_funct(fpga_in_buffer, mcu_in_buffer, fpga_out_buffer, mcu_out_buffer, mcubus, fpgabus);
            std::this_thread::yield();
        } });
    while (!host_delete) {
        std::this_thread::yield();
        // HOST THREAD
        if (host && !host_delete && !client && client_delete)
            if (!delete_mcu.test_and_set()) {
                delete host;
                host = nullptr;
                host_delete = true;
            }
        // CLIENT THREAD
        if (client && !client_delete)
            if (!delete_fpga.test_and_set()) {
                delete client;
                client = nullptr;
                client_delete = true;
            }
    }
    if (client)
        delete client;
    if (host)
        delete host;
    handshake_stop = true;
    handshake.join();
    remove(pidPath);
    return 0;
}
