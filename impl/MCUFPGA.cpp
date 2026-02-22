/*
 * Buffers: 12288b=>3Pages (4KiB nVidia Packet 3 WORD); IRQs with async or HandShake and device mmap
 * PunchCards: 24b=>3Double (64bit CPU 3 WORD), 12b=>3Int (32bit CPU 3 WORD); Poll In/Out Notifier with async
 * WithinFGPA: 3b=>TrueColor (8bit MCU 3 WORD); RTL HandShake with thread
*/
#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/serial_helpers.hpp"
#include <future>
#include "software-on-silicon/SerialNotifier.hpp"
#include "software-on-silicon/cpp14.hpp"
#include <string>
#include "software-on-silicon/ByteWiseTransfer.hpp"
#include "software-on-silicon/Serial.hpp"
#define COM_BUFFER std::array<unsigned char, 1>
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/MCUFPGA.hpp"
#include "software-on-silicon/mcufpga_helpers.hpp"

#include "MCUFPGA/DMA.cpp"
#include "MCUFPGA/TrueColor.cpp"

class FPGAProcessingSwitch : public SOS::Behavior::SerialProcessing {
public:
    using bus_type = typename SOS::MemoryView::SerialProcessNotifier<TrueColor, DMA, DMA>;
    FPGAProcessingSwitch(bus_type& bus)
        : _nBus(bus)
        , counterBus(std::get<0>(_nBus.objects))
        , SOS::Behavior::SerialProcessing(bus)
    {
        counterBus.signal.getAcknowledgeRef().clear();
        //LOCK
        auto fut = write_status[1].get();
        if (fut) {
            //EDIT
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
            //SEND
            if (!write_status[1].valid()) {
                sync[1] = true;
                write_status[1] = std::async(std::launch::async, &SOS::Protocol::write_status, std::ref(write_fault[1]), std::ref(write_ack[1]));
                //CALLBACK
                auto t = std::thread(&dump<DMA&>, std::move(write_status[1].share()), std::ref(std::get<1>(_nBus.objects)));
                t.detach();
            } else {
                SFA::util::logic_error(SFA::util::error_code::TypeOfFutureHasBeenModifiedDuringEdit, __FILE__, __func__, typeid(*this).name());
            }
        } else {
            SFA::util::logic_error(SFA::util::error_code::WriteRequestHasBeenCanceledByOtherSide, __FILE__, __func__, typeid(*this).name());
        }
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~FPGAProcessingSwitch()
    {
        SOS::Behavior::Loop::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    void process_hook() {
        //SIGNALING
        if (!read_ack[0].test_and_set()){
            if (!counterBus.signal.getAcknowledgeRef().test_and_set()) {
                counterBus.signal.getUpdatedRef().clear();
            }
        }
        //COMPUTE
        if (!counterBus.signal.getUpdatedRef().test_and_set()){
            auto red = std::get<0>(std::get<0>(counterBus.const_cables));
            (*red)++;
            auto blue = std::get<1>(std::get<0>(counterBus.const_cables));
            (*(--blue))++;
            sync[0] = true;
            counterBus.signal.getAcknowledgeRef().clear();
        }
    }

private:
    bus_type& _nBus;
    SOS::Protocol::CharBusGenerator<std::tuple_element<0,decltype(bus_type::objects)>::type> counterBus;
    std::thread _thread = std::thread {};
};
class MCUProcessingSwitch : public SOS::Behavior::SerialProcessing {
public:
    using bus_type = typename SOS::MemoryView::SerialProcessNotifier<TrueColor, DMA, DMA>;
    MCUProcessingSwitch(bus_type& bus)
        : _nBus(bus)
        , counterBus(std::get<0>(_nBus.objects))
        , SOS::Behavior::SerialProcessing(bus)
    {
        counterBus.signal.getUpdatedRef().clear();
        //LOCK
        auto fut = write_status[2].get();
        //EDIT
        //if (fut){
        std::get<2>(_nBus.objects).fill('-');
        //SEND
        if (!write_status[2].valid()) {
            sync[2] = true;
            write_status[2] = std::async(std::launch::async, &SOS::Protocol::write_status, std::ref(write_fault[2]), std::ref(write_ack[2]));
            //CALLBACK
            auto t = std::thread(&dump<DMA&>, std::move(write_status[2].share()), std::ref(std::get<2>(_nBus.objects)));
            t.detach();
        } else {
            SFA::util::logic_error(SFA::util::error_code::TypeOfFutureHasBeenModifiedDuringEdit, __FILE__, __func__, typeid(*this).name());
        }
        //}
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~MCUProcessingSwitch()
    {
        SOS::Behavior::Loop::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing::event_loop(); }
    void process_hook() {
        //SIGNALING
        if (!read_ack[0].test_and_set()){
            if (!counterBus.signal.getAcknowledgeRef().test_and_set()) {
                counterBus.signal.getUpdatedRef().clear();
            }
        }
        //COMPUTE
        if (!counterBus.signal.getUpdatedRef().test_and_set()){
            auto red = std::get<0>(std::get<0>(counterBus.const_cables));
            (*red)--;
            auto green = std::get<0>(std::get<0>(counterBus.const_cables));
            (*(++green))++;
            sync[0] = true;
            counterBus.signal.getAcknowledgeRef().clear();
        }
    }

private:
    bus_type& _nBus;
    SOS::Protocol::CharBusGenerator<std::tuple_element<0,decltype(bus_type::objects)>::type> counterBus;
    std::thread _thread = std::thread {};
};
class FPGA : public SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, TrueColor, DMA, DMA> {
public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    FPGA(bus_type& myBus)
        : SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, TrueColor, DMA, DMA>(myBus)
    {
        boot_time = std::chrono::high_resolution_clock::now();
        std::cout << "FPGA Color " << std::get<0>(_foreign.objects) << std::endl;
        _thread = SOS::Behavior::Stoppable::start(this);
    }
    ~FPGA()
    {
        // while (!exit_query())
        //     std::this_thread::yield();
        SOS::Behavior::Stoppable::destroy(_thread);
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout << "FPGA Color " << std::get<0>(_foreign.objects) << std::endl;
        std::cout << "Dumping FPGA DMA Objects" << std::endl;
        dump_objects(_foreign.objects, rx_counter, tx_counter, boot_time, kill_time);
        if (SOS::Protocol::Serial<TrueColor, DMA, DMA>::reads_pending())
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
class MCU : public SOS::Behavior::SimulationMCU<MCUProcessingSwitch, TrueColor, DMA, DMA> {
public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    MCU(bus_type& myBus)
        : SOS::Behavior::SimulationMCU<MCUProcessingSwitch, TrueColor, DMA, DMA>(myBus)
    {
        boot_time = std::chrono::high_resolution_clock::now();
        std::cout << "MCU Color " << std::get<0>(_foreign.objects) << std::endl;
        _thread = SOS::Behavior::Stoppable::start(this);
    }
    ~MCU()
    {
        // while (!exit_query())
        //     std::this_thread::yield();
        SOS::Behavior::Stoppable::destroy(_thread);
        kill_time = std::chrono::high_resolution_clock::now();
        std::cout << "MCU Color " << std::get<0>(_foreign.objects) << std::endl;
        std::cout << "Dumping MCU DMA Objects" << std::endl;
        dump_objects(_foreign.objects, rx_counter, tx_counter, boot_time, kill_time);
        if (SOS::Protocol::Serial<TrueColor, DMA, DMA>::reads_pending())
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
