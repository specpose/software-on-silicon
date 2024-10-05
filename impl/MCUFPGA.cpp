#include <iostream>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/serial_helpers.hpp"
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
        _thread = SOS::Behavior::Stoppable::start(this);
    }
    ~FPGAProcessingSwitch()
    {
        SOS::Behavior::Stoppable::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    virtual void start() final { _thread = SOS::Behavior::Stoppable::start(this); }
    void read_notify_hook()
    {
        auto object_id = std::get<0>(_nBus.cables).getReadDestinationRef().load();
        switch (object_id)
        {
        case 0:
            // fresh out of read_lock, safe before unsynced
            if (!std::get<0>(_nBus.objects).mcu_owned())
            { // WRITE-LOCK encapsulated <= Not all implementations need a write-lock
                auto n = std::get<0>(_nBus.objects).getNumber();
                std::get<0>(_nBus.objects).setNumber(++n);
                std::get<0>(_nBus.objects).set_mcu_owned(true);
                _nBus.descriptors[0].synced = false;
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
        auto object_id = std::get<0>(_nBus.cables).getWriteOriginRef().load();
        switch (object_id)
        {
        case 0:
            // just been synced, can now process further
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
        _thread = SOS::Behavior::Stoppable::start(this);
    }
    ~MCUProcessingSwitch()
    {
        SOS::Behavior::Stoppable::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    virtual void start() final { throw SFA::util::runtime_error("Testing MCU hotplug: MCU ProcessingSwitch relaunched after com_hotplug_action.", __FILE__, __func__);}
    void read_notify_hook()
    {
        auto object_id = std::get<0>(_nBus.cables).getReadDestinationRef().load();
        switch (object_id)
        {
        case 0:
            if (std::get<0>(_nBus.objects).mcu_owned())
            { // WRITE-LOCK encapsulated <= Not all implementations need a write-lock
                auto n = std::get<0>(_nBus.objects).getNumber();
                std::get<0>(_nBus.objects).setNumber(++n);
                std::get<0>(_nBus.objects).set_mcu_owned(false);
                _nBus.descriptors[0].synced = false;
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
        auto object_id = std::get<0>(_nBus.cables).getWriteOriginRef().load();
        switch (object_id)
        {
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
        destroy(_thread);
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout << "FPGA read notify count " << std::get<0>(_foreign.objects).getNumber() << std::endl;
        std::cout << "Dumping FPGA DMA Objects" << std::endl;
        dump_objects(_foreign.objects, _foreign.descriptors, boot_time, kill_time);
    }
    void requestStop()//Only from Ctrl-C
    {

        SOS::Behavior::BootstrapEventController<FPGAProcessingSwitch>::stop_children();
        loop_shutdown = true;
    };
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
        SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::resend_current_object();
        SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::clear_read_receive();
    }
    virtual void com_shutdown_action() final
    {
        SOS::Behavior::Stoppable::request_stop();
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
        destroy(_thread);
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout << "MCU read notify count " << std::get<0>(_foreign.objects).getNumber() << std::endl;
        std::cout << "Dumping MCU DMA Objects" << std::endl;
        dump_objects(_foreign.objects, _foreign.descriptors, boot_time, kill_time);
    }
    void requestStop()//Only from Ctrl-C
    {
        SOS::Behavior::BootstrapEventController<MCUProcessingSwitch>::stop_children();
        loop_shutdown = true;
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
        SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::resend_current_object();
        SOS::Protocol::Serial<SymbolRateCounter, DMA, DMA>::clear_read_receive();
        if (!std::get<0>(_foreign.objects).mcu_owned()){
            std::get<0>(_foreign.objects).set_mcu_owned(false);
            _foreign.descriptors[0].synced = false;
	}
    }
    virtual void com_shutdown_action() final
    {
        SOS::Behavior::Stoppable::request_stop();//No hotplug
    }

private:
    bool stateOfObjectZero = false;
    bool syncStateObjectZero = true;

    std::chrono::time_point<std::chrono::high_resolution_clock> boot_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> kill_time;
    std::thread _thread = std::thread{};
};
