#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/serial_helpers.hpp"
#include "software-on-silicon/SerialNotifier.hpp"
#include <string>
#include "software-on-silicon/ByteWiseTransfer.hpp"
#include "software-on-silicon/Serial.hpp"
#define COM_BUFFER std::array<unsigned char, 1>
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MCUFPGA.hpp"
#include "software-on-silicon/mcufpga_helpers.hpp"
#include "MCUFPGA/DMA.cpp"

#include "MCUFPGA/SymbolRateCounter.cpp"
class FPGAProcessingSwitch : public SOS::Behavior::SerialProcessing {
public:
    using bus_type = typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA>;
    FPGAProcessingSwitch(bus_type& bus)
        : _nBus(bus)
        , SOS::Behavior::SerialProcessing(bus)
    {
        int writeBlinkCounter = 0;
        bool writeBlink = true;
        for (std::size_t i = 0; i < std::get<1>(_nBus.objects).size(); i++) {
            DMA::value_type data;
            if (writeBlink)
                data = 42; //'*'
            else
                data = 95; //'_'
            std::get<1>(_nBus.objects)[i] = data;
            writeBlinkCounter++;
            if (writeBlink && writeBlinkCounter == 333) {
                writeBlink = false;
                writeBlinkCounter = 0;
            } else if (!writeBlink && writeBlinkCounter == 666) {
                writeBlink = true;
                writeBlinkCounter = 0;
            }
        }
        sync[1] = true;
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~FPGAProcessingSwitch()
    {
        SOS::Behavior::Loop::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    void read_notify_hook(std::size_t object_id)
    {
        switch (object_id) {
        case 0:
            // fresh out of read_lock, safe before unsynced
            if (!read[0]) {
                auto n = std::get<0>(_nBus.objects).getNumber();
                if (!std::get<0>(_nBus.objects).mcu_owned()) { // WRITE-LOCK encapsulated <= Not all implementations need a write-lock
                    if (!sync[0] && !write[0]) {
                        std::get<0>(_nBus.objects).setNumber(++n);
                        std::get<0>(_nBus.objects).set_mcu_owned(true);
                        sync[0] = true;
                    }
                }
            } else {
                SFA::util::runtime_error(SFA::util::error_code::FPGAProcessingSwitchThreadIsTooSlow, __FILE__, __func__, typeid(*this).name());
            }
            break;
        case 1:
            break;
        case 2:
            break;
        }
    }
    void write_notify_hook(std::size_t object_id)
    {
        switch (object_id) {
        // just been transfered, can now process further
        case 0:
            break;
        case 1:
            break;
        case 2:
            break;
        }
    }

private:
    bus_type& _nBus;
    std::thread _thread = std::thread {};
};
class MCUProcessingSwitch : public SOS::Behavior::SerialProcessing {
public:
    using bus_type = typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA>;
    MCUProcessingSwitch(bus_type& bus)
        : _nBus(bus)
        , SOS::Behavior::SerialProcessing(bus)
    {
        std::get<0>(_nBus.objects).setNumber(0);
        std::get<0>(_nBus.objects).set_mcu_owned(false);
        sync[0] = true;
        std::get<2>(_nBus.objects).fill('-');
        sync[2] = true;
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~MCUProcessingSwitch()
    {
        SOS::Behavior::Loop::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    void read_notify_hook(std::size_t object_id)
    {
        switch (object_id) {
        case 0:
            // fresh out of read_lock, safe before unsynced
            if (!read[0]) {
                auto n = std::get<0>(_nBus.objects).getNumber();
                if (std::get<0>(_nBus.objects).mcu_owned()) { // WRITE-LOCK encapsulated <= Not all implementations need a write-lock
                    if (!sync[0] && !write[0]) {
                        std::get<0>(_nBus.objects).setNumber(++n);
                        std::get<0>(_nBus.objects).set_mcu_owned(false);
                        sync[0] = true;
                    }
                }
            } else {
                SFA::util::runtime_error(SFA::util::error_code::MCUProcessingSwitchThreadIsTooSlow, __FILE__, __func__, typeid(*this).name());
            }
            break;
        case 1:
            break;
        case 2:
            break;
        }
    }
    void write_notify_hook(std::size_t object_id)
    {
        switch (object_id) {
        // just been transfered, can now process further
        case 0:
            break;
        case 1:
            break;
        case 2:
            break;
        }
    }

private:
    bus_type& _nBus;
    std::thread _thread = std::thread {};
};
class FPGA : public SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, SymbolRateCounter, DMA, DMA> {
public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    FPGA(bus_type& myBus)
        : SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, SymbolRateCounter, DMA, DMA>(myBus)
    {
        boot_time = std::chrono::high_resolution_clock::now();
        _thread = SOS::Behavior::Stoppable::start(this);
    }
    ~FPGA()
    {
        // while (!exit_query())
        //     std::this_thread::yield();
        SOS::Behavior::Stoppable::destroy(_thread);
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout << "FPGA read notify count " << std::get<0>(_foreign.objects).getNumber() << std::endl;
        std::cout << "Dumping FPGA DMA Objects" << std::endl;
        dump_objects(_foreign.objects, rx_counter, tx_counter, boot_time, kill_time);
        if (SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::reads_pending())
            SFA::util::runtime_error(SFA::util::error_code::ReadsPendingAfterComthreadDestruction, __FILE__, __func__, typeid(*this).name());
    }
    virtual void request_shutdown_action() final // Only from Ctrl-C
    {
        stop_notifier();
    };
    virtual void com_hotplug_action() final
    {
        this->clear_read_receive();
    }
    virtual bool exit_query() final
    {
        if (_vars.received_sighup)
            return true;
        return false;
    }
    virtual void com_shutdown_action() final
    {
    }
    virtual void com_sighup_action() final
    {
    }
    virtual bool incoming_shutdown_query() final
    {
        if (!transfers_pending() && !_vars.acknowledgeRequested && !_vars.received_acknowledge && descendants_stopped())
            return true;
        return false;
    }
    virtual bool outgoing_sighup_query() final
    {
        if (_vars.sent_com_shutdown && _vars.received_com_shutdown && !reads_pending() && !writes_pending())
            return true;
        return false;
    }

private:
    bool stateOfObjectOne = false;
    bool syncStateObjectOne = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread {};
};
class MCU : public SOS::Behavior::SimulationMCU<MCUProcessingSwitch, SymbolRateCounter, DMA, DMA> {
public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    MCU(bus_type& myBus)
        : SOS::Behavior::SimulationMCU<MCUProcessingSwitch, SymbolRateCounter, DMA, DMA>(myBus)
    {
        boot_time = std::chrono::high_resolution_clock::now();
        _thread = SOS::Behavior::Stoppable::start(this);
    }
    ~MCU()
    {
        // while (!exit_query())
        //     std::this_thread::yield();
        SOS::Behavior::Stoppable::destroy(_thread);
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout << "MCU read notify count " << std::get<0>(_foreign.objects).getNumber() << std::endl;
        std::cout << "Dumping MCU DMA Objects" << std::endl;
        dump_objects(_foreign.objects, rx_counter, tx_counter, boot_time, kill_time);
        if (SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::reads_pending())
            SFA::util::runtime_error(SFA::util::error_code::ReadsPendingAfterComthreadDestruction, __FILE__, __func__, typeid(*this).name());
    }
    virtual void request_shutdown_action() final // Only from Ctrl-C
    {
    }
    virtual void com_hotplug_action() final
    {
        this->clear_read_receive();
        // if (!std::get<0>(_foreign.objects).mcu_owned()){
        //     std::get<0>(_foreign.objects).set_mcu_owned(false);
        //     sync[0] = true;
        // }
    }
    virtual bool exit_query() final
    {
        if (_vars.received_sighup)
            return true;
        return false;
    }
    virtual void com_shutdown_action() final
    {
        stop_notifier();
    }
    virtual void com_sighup_action() final
    {
    }
    virtual bool incoming_shutdown_query() final
    {
        if (_vars.received_com_shutdown && !transfers_pending() && !_vars.acknowledgeRequested && !_vars.received_acknowledge && descendants_stopped())
            return true;
        return false;
    }
    virtual bool outgoing_sighup_query() final
    {
        if (_vars.received_sighup && !reads_pending() && !writes_pending())
            return true;
        return false;
    }

private:
    bool stateOfObjectZero = false;
    bool syncStateObjectZero = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread {};
};
