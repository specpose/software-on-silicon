#include <iostream>
#include "software-on-silicon/error.hpp"
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
class FPGAProcessingSwitch : public SOS::Behavior::SerialProcessing, public SOS::Behavior::BootstrapDummyEventController<>
{
public:
    using bus_type = typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA>;
    FPGAProcessingSwitch(bus_type &bus) : _nBus(bus), SOS::Behavior::SerialProcessing(), SOS::Behavior::BootstrapDummyEventController<>(bus.signal)
    {
        if (_nBus.descriptors.size()!=3)//TODO also assert on tuple
            SFA::util::runtime_error(SFA::util::error_code::DMADescriptorsInitializationFailed, __FILE__, __func__, typeid(*this).name());
        _thread = SOS::Behavior::Stoppable::start(this);
    }
    ~FPGAProcessingSwitch()
    {
        SOS::Behavior::Stoppable::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    virtual void restart() final { _thread = SOS::Behavior::Stoppable::start(this); }
    void read_notify_hook()
    {
        auto object_id = std::get<0>(_nBus.cables).getReceiveNotificationRef().load();
        switch (object_id)
        {
        case 0:
            // fresh out of read_lock, safe before unsynced
            if (!_nBus.descriptors[0].readLock){
                auto n = std::get<0>(_nBus.objects).getNumber();
                if (!std::get<0>(_nBus.objects).mcu_owned())
                { // WRITE-LOCK encapsulated <= Not all implementations need a write-lock
                    if (_nBus.descriptors[0].synced){
                        std::get<0>(_nBus.objects).setNumber(++n);
                        std::get<0>(_nBus.objects).set_mcu_owned(true);
                        _nBus.descriptors[0].synced = false;
                    }
                }
            } else {
                SFA::util::runtime_error(SFA::util::error_code::FPGAProcessingSwitchThreadIsTooSlow,__FILE__,__func__, typeid(*this).name());
            }
            break;
        case 1:
            break;
        case 2:
            break;
        }
    }
    void write_notify_hook()
    {
        auto object_id = std::get<0>(_nBus.cables).getSendNotificationRef().load();
        switch (object_id)
        {
        // just been transfered, can now process further
        case 0:
            break;
        case 1:
            break;
        case 2:
            break;
        }
    }

protected:
    virtual bool received() final
    {
        return !_intrinsic.getUpdatedRef().test_and_set();
    }
    virtual bool transfered() final
    {
        return !_intrinsic.getAcknowledgeRef().test_and_set();
    }
    virtual bool is_running() final { return SOS::Behavior::Stoppable::is_running(); }
    virtual void finished() final { SOS::Behavior::Stoppable::finished(); }

private:
    bus_type &_nBus;
    std::thread _thread = std::thread{};
};
class MCUProcessingSwitch : public SOS::Behavior::SerialProcessing, public SOS::Behavior::BootstrapDummyEventController<>
{
public:
    using bus_type = typename SOS::MemoryView::SerialProcessNotifier<SymbolRateCounter, DMA, DMA>;
    MCUProcessingSwitch(bus_type &bus) : _nBus(bus), SOS::Behavior::SerialProcessing(), SOS::Behavior::BootstrapDummyEventController<>(bus.signal)
    {
        if (_nBus.descriptors.size()!=3)//TODO also assert on tuple
            SFA::util::runtime_error(SFA::util::error_code::DMADescriptorsInitializationFailed, __FILE__, __func__, typeid(*this).name());
        _thread = SOS::Behavior::Stoppable::start(this);
    }
    ~MCUProcessingSwitch()
    {
        SOS::Behavior::Stoppable::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    virtual void restart() final { SFA::util::runtime_error(SFA::util::error_code::MCUProcessingSwitchRelaunchedAfterComHotplugAction, __FILE__, __func__, typeid(*this).name());}
    void read_notify_hook()
    {
        auto object_id = std::get<0>(_nBus.cables).getReceiveNotificationRef().load();
        switch (object_id)
        {
        case 0:
            // fresh out of read_lock, safe before unsynced
            if (!_nBus.descriptors[0].readLock){
                auto n = std::get<0>(_nBus.objects).getNumber();
                if (std::get<0>(_nBus.objects).mcu_owned())
                { // WRITE-LOCK encapsulated <= Not all implementations need a write-lock
                    if (_nBus.descriptors[0].synced){
                        std::get<0>(_nBus.objects).setNumber(++n);
                        std::get<0>(_nBus.objects).set_mcu_owned(false);
                        _nBus.descriptors[0].synced = false;
                    }
                }
            } else {
                SFA::util::runtime_error(SFA::util::error_code::MCUProcessingSwitchThreadIsTooSlow,__FILE__,__func__, typeid(*this).name());
            }
            break;
        case 1:
            break;
        case 2:
            break;
        }
    }
    void write_notify_hook()
    {
        auto object_id = std::get<0>(_nBus.cables).getSendNotificationRef().load();
        switch (object_id)
        {
        // just been transfered, can now process further
        case 0:
            break;
        case 1:
            break;
        case 2:
            break;
        }
    }

protected:
    virtual bool received() final
    {
        return !_intrinsic.getUpdatedRef().test_and_set();
    }
    virtual bool transfered() final
    {
        return !_intrinsic.getAcknowledgeRef().test_and_set();
    }
    virtual bool is_running() final { return SOS::Behavior::Stoppable::is_running(); }
    virtual void finished() final { SOS::Behavior::Stoppable::finished(); }

private:
    bus_type &_nBus;
    std::thread _thread = std::thread{};
};
class FPGA : public SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, SymbolRateCounter, DMA, DMA>
{
public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    FPGA(bus_type &myBus) : SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, SymbolRateCounter, DMA, DMA>(myBus)
    {
        int writeBlinkCounter = 0;
        bool writeBlink = true;
        for (std::size_t i = 0; i < std::get<1>(_foreign.objects).size(); i++)
        {
            DMA::value_type data;
            if (writeBlink)
                data = 42; //'*'
            else
                data = 95; //'_'
            std::get<1>(_foreign.objects)[i] = data;
            writeBlinkCounter++;
            if (writeBlink && writeBlinkCounter == 333)
            {
                writeBlink = false;
                writeBlinkCounter = 0;
            }
            else if (!writeBlink && writeBlinkCounter == 666)
            {
                writeBlink = true;
                writeBlinkCounter = 0;
            }
        }
        foreign().descriptors[1].synced = false;
        boot_time = std::chrono::high_resolution_clock::now();
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~FPGA()
    {
        //while (SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::reads_pending())
        //    std::this_thread::yield();
        destroy(_thread);
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout << "FPGA read notify count " << std::get<0>(_foreign.objects).getNumber() << std::endl;
        std::cout << "Dumping FPGA DMA Objects" << std::endl;
        dump_objects(_foreign.objects, _foreign.descriptors, boot_time, kill_time);
        if (SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::reads_pending())
            SFA::util::runtime_error(SFA::util::error_code::ReadsPendingAfterComthreadDestruction,__FILE__,__func__, typeid(*this).name());
    }
    void requestStop()//Only from Ctrl-C
    {
        stop_notifier();
        _vars.loop_shutdown = true;//no more transfers or syncs? then sent_com_shutdown
    };
    void restart() {
        _thread = SOS::Behavior::Loop::start(this);
    }
    bool isStopped()
    {
        if (is_finished())
        {
            finished();
            return true;
        }
        return false;
    }
    virtual void com_hotplug_action() final
    {
        this->clear_read_receive();
        this->resend_current_object();
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
    virtual void shutdown_action() final
    {
        SOS::Behavior::Stoppable::request_stop();
    }
    virtual bool incoming_shutdown_query() final
    {
        if (_vars.loop_shutdown && !transfers_pending() && !_vars.acknowledgeRequested && !_vars.received_acknowledge)
            return true;
        return false;
    }
    virtual bool outgoing_sighup_query() final
    {
        if (_vars.received_sighup && (_vars.received_com_shutdown && _vars.received_idle) && !writes_pending())
            return true;
        return false;
    }

private:
    bool stateOfObjectOne = false;
    bool syncStateObjectOne = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};
class MCU : public SOS::Behavior::SimulationMCU<MCUProcessingSwitch, SymbolRateCounter, DMA, DMA>
{
public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    MCU(bus_type &myBus) : SOS::Behavior::SimulationMCU<MCUProcessingSwitch, SymbolRateCounter, DMA, DMA>(myBus)
    {
        std::get<0>(_foreign.objects).setNumber(0);
        std::get<0>(_foreign.objects).set_mcu_owned(false);
        foreign().descriptors[0].synced = false;
        std::get<2>(_foreign.objects).fill('-');
        foreign().descriptors[2].synced = false;
        boot_time = std::chrono::high_resolution_clock::now();
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~MCU()
    {
        //while (SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::reads_pending())
        //    std::this_thread::yield();
        destroy(_thread);
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout << "MCU read notify count " << std::get<0>(_foreign.objects).getNumber() << std::endl;
        std::cout << "Dumping MCU DMA Objects" << std::endl;
        dump_objects(_foreign.objects, _foreign.descriptors, boot_time, kill_time);
        if (SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::reads_pending())
            SFA::util::runtime_error(SFA::util::error_code::ReadsPendingAfterComthreadDestruction,__FILE__,__func__, typeid(*this).name());
    }
    void requestStop()//Only from Ctrl-C
    {
        _vars.loop_shutdown = true;
    }
    void restart() {
        _thread = SOS::Behavior::Loop::start(this);
    }
    bool isStopped()
    {
        if (is_finished())
        {
            finished();
            return true;
        }
        return false;
    };
    virtual void com_hotplug_action() final
    {
        this->clear_read_receive();
        this->resend_current_object();
        //if (!std::get<0>(_foreign.objects).mcu_owned()){
        //    std::get<0>(_foreign.objects).set_mcu_owned(false);
        //    _foreign.descriptors[0].synced = false;
        //}
    }
    virtual bool exit_query() final
    {
        if (_vars.sent_sighup)
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
    virtual void shutdown_action() final
    {
        SOS::Behavior::Stoppable::request_stop();
    }
    virtual bool incoming_shutdown_query() final
    {
        if (_vars.received_com_shutdown && !transfers_pending() && !_vars.acknowledgeRequested && !_vars.received_acknowledge)
            return true;
        return false;
    }
    virtual bool outgoing_sighup_query() final
    {
        if (_vars.sent_com_shutdown && (_vars.received_com_shutdown && _vars.received_idle) && !writes_pending())
            return true;
        return false;
    }

private:
    bool stateOfObjectZero = false;
    bool syncStateObjectZero = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};
