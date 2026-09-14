/*
 * Buffers: 12288b=>3Pages (4KiB nVidia Packet 3 WORD); IRQs with async or HandShake and device mmap
 * PunchCards: 24b=>3Double (64bit CPU 3 WORD), 12b=>3Int (32bit CPU 3 WORD); Poll In/Out Notifier with async
 * WithinFGPA: 3b=>TrueColorClass (8bit MCU 3 WORD); RTL HandShake with thread
 */
#include <iostream>
#include "error.cpp"
#include "software-on-silicon/INTERFACE.hpp"
#include "software-on-silicon/serial_helpers.hpp"
#include <future>
#include "software-on-silicon/DMADescriptor.hpp"
#include "software-on-silicon/cpp11.hpp"
#include "software-on-silicon/SerialNotifier.hpp"
#include <string>
#include "software-on-silicon/rtos_helpers.hpp"
#include "software-on-silicon/ByteWiseTransfer.hpp"
#include "MCUFPGA/DMA.cpp"
#include "MCUFPGA/TrueColor.cpp"
#include "software-on-silicon/mcufpga_helpers.hpp"
#include "ByteWiseTransfer.cpp"
#include "software-on-silicon/Serial.hpp"
#define COM_BUFFER std::array<unsigned char, 1>
#include "software-on-silicon/MCUFPGA.hpp"

class FPGASimpleDummy : public SOS::Behavior::SequentialResolver<TrueColorClass, DMA, DMA> {
public:
    FPGASimpleDummy(bus_type& bus)
        : SOS::Behavior::SequentialResolver<TrueColorClass, DMA, DMA>(bus)

    {
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~FPGASimpleDummy(){
        SOS::Behavior::Loop::destroy(_thread);
    }
    void event_loop()
    {
        // SIGNALING
        resolve(0);
        if (!_intrinsic.getNotifyRef().test_and_set()) {
        if (!read[0].ready.test_and_set()) {
            if (read[0].result) {
            //check ownership
            //std::get<0>(bus.objs).red++; // Hack
            //std::get<0>(bus.objs).blue++; // Hack
            //auto red = reinterpret_cast<unsigned char*>(&doubleBuffer[0][0]);
            //(*red)--;
            //auto blue = reinterpret_cast<unsigned char*>(&doubleBuffer[0][2]);
            //(*blue)++;
            _intrinsic[0].sync_me.clear();
            }
        }
        if (!write[0].ready.test_and_set()) {
            if (!write[0].result) {
                //if (obj_id == 1 || obj_id == 2) {
                std::cout << typeid(*this).name() << ": write of object id " << 0 << " canceled" << std::endl;
                //}
            }
        }
        }
        std::this_thread::yield();
    }
private:
    std::thread _thread;
};
class FPGAProcessingSwitch : public SOS::Behavior::SerialProcessing<FPGASimpleDummy> {
public:
    FPGAProcessingSwitch(bus_type& bus)
        : SOS::Behavior::SerialProcessing<FPGASimpleDummy>(bus)
    {
        //counterBus.signal.getAcknowledgeRef().clear();
        // LOCK
        /*auto fut = write_status[1].get();
        if (fut) {
            // EDIT
            int writeBlinkCounter = 0;
            bool writeBlink = true;
            for (std::size_t i = 0; i < sizeof(std::get<1>(_sBus.objects)); i++) {
                std::get<1>(_sBus.objects)[i] = writeBlink? '*' : '_';
                writeBlinkCounter++;
                if (writeBlink && writeBlinkCounter == 84) {
                    writeBlink = false;
                    writeBlinkCounter = 0;
                } else if (!writeBlink && writeBlinkCounter == 168) {
                    writeBlink = true;
                    writeBlinkCounter = 0;
                }
            }
            // SEND
            if (!write_status[1].valid()) {
                bus.sync_id[1] = true;
                write_status[1] = std::async(std::launch::async, &SOS::Protocol::async_status, std::ref(write_fault[1]), std::ref(write_ack[1]));
                // CALLBACK
                auto t = std::thread(&dump<DMA>, std::move(write_status[1].share()), std::ref(std::get<1>(_sBus.objects)));
                t.detach();
            } else {
                SFA::util::logic_error(SFA::util::error_code::TypeOfFutureHasBeenModifiedDuringEdit, __FILE__, __func__, typeid(*this).name());
            }
        } else {
            SFA::util::logic_error(SFA::util::error_code::WriteRequestHasBeenCanceledByOtherSide, __FILE__, __func__, typeid(*this).name());
        }*/
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~FPGAProcessingSwitch()
    {
        SOS::Behavior::Loop::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing<FPGASimpleDummy>::event_loop(); }

private:
    std::thread _thread;
};
class MCUSimpleDummy : public SOS::Behavior::SequentialResolver<TrueColorClass, DMA, DMA> {
public:
    MCUSimpleDummy(bus_type& bus)
        : SOS::Behavior::SequentialResolver<TrueColorClass, DMA, DMA>(bus)
    {
        _intrinsic[0].sync_me.clear();
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~MCUSimpleDummy(){
        SOS::Behavior::Loop::destroy(_thread);
    }
    void event_loop()
    {
        // SIGNALING
        resolve(0);
        if (!_intrinsic.getNotifyRef().test_and_set()) {
        if (!read[0].ready.test_and_set()) {
            if (read[0].result) {
            //check ownership
            //std::get<0>(bus.objs).red--;
            //std::get<0>(bus.objs).green++;
            //auto red = reinterpret_cast<unsigned char*>(&doubleBuffer[0][0]);
            //(*red)++;
            //auto green = reinterpret_cast<unsigned char*>(&doubleBuffer[0][1]);
            //(*green)++;
            _intrinsic[0].sync_me.clear();
            }
        }
        if (!write[0].ready.test_and_set()) {
            if (!write[0].result) {
                //if (obj_id == 1 || obj_id == 2) {
                std::cout << typeid(*this).name() << ": write of object id " << 0 << " canceled" << std::endl;
                //}
            }
        }
        }
        std::this_thread::yield();
    }
private:
    std::thread _thread;
};
class MCUProcessingSwitch : public SOS::Behavior::SerialProcessing<MCUSimpleDummy> {
public:
    MCUProcessingSwitch(bus_type& bus)
        : SOS::Behavior::SerialProcessing<MCUSimpleDummy>(bus)
    {
        //counterBus.signal.getUpdatedRef().clear();
        // LOCK
        /*auto fut = write_status[2].get();
        // EDIT
        // if (fut){
        std::fill(reinterpret_cast<unsigned char*>(&std::get<2>(_sBus.objects)),reinterpret_cast<unsigned char*>(&std::get<2>(_sBus.objects))+sizeof(std::get<2>(_sBus.objects)),'-');
        // SEND
        if (!write_status[2].valid()) {
            bus.sync_id[2] = true;
            write_status[2] = std::async(std::launch::async, &SOS::Protocol::async_status, std::ref(write_fault[2]), std::ref(write_ack[2]));
            // CALLBACK
            auto t = std::thread(&dump<DMA>, std::move(write_status[2].share()), std::ref(std::get<2>(_sBus.objects)));
            t.detach();
        } else {
            SFA::util::logic_error(SFA::util::error_code::TypeOfFutureHasBeenModifiedDuringEdit, __FILE__, __func__, typeid(*this).name());
        }
        //}*/
        _thread = SOS::Behavior::Loop::start(this);
    }
    ~MCUProcessingSwitch()
    {
        SOS::Behavior::Loop::destroy(_thread);
    }
    virtual void event_loop() final { SOS::Behavior::SerialProcessing<MCUSimpleDummy>::event_loop(); }

private:
    std::thread _thread;
};
class FPGA : public SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, TrueColorClass, DMA, DMA> {
public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    FPGA(bus_type& myBus)
        : SOS::Behavior::SimulationFPGA<FPGAProcessingSwitch, TrueColorClass, DMA, DMA>(myBus)
    {
        boot_time = std::chrono::high_resolution_clock::now();
        //std::cout << "FPGA Color " << std::get<0>(_foreign.objects) << std::endl;
        _thread = SOS::Behavior::Loop::start(this);
    }
    virtual ~FPGA() final
    {
        //Interface
        this->stop_notifier();
        while (!exit_query())
            std::this_thread::yield();
        std::cout << typeid(*this).name() << " shutdown" << std::endl;
        SOS::Behavior::Loop::destroy(_thread);
        //Debug
        kill_time = std::chrono::high_resolution_clock::now();
        //std::cout << "FPGA Color " << std::get<0>(_foreign.objects) << std::endl;
        std::cout << "Dumping FPGA DMA Objects" << std::endl;
        dump_descriptors_binary(this->descriptors, rx_counter, tx_counter, boot_time, kill_time);
        if (SOS::Protocol::Serial<FPGAProcessingSwitch, TrueColorClass, DMA, DMA>::reads_pending())
            SFA::util::runtime_error(SFA::util::error_code::ReadsPendingAfterComthreadDestruction, __FILE__, __func__, typeid(*this).name());
    }
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
        if (!transfers_pending() && !_vars.acknowledgeRequested && !_vars.received_acknowledge && _vars.descendants_notified)
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
    std::thread _thread;
};
class MCU : public SOS::Behavior::SimulationMCU<MCUProcessingSwitch, TrueColorClass, DMA, DMA> {
public:
    using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
    MCU(bus_type& myBus)
        : SOS::Behavior::SimulationMCU<MCUProcessingSwitch, TrueColorClass, DMA, DMA>(myBus)
    {
        boot_time = std::chrono::high_resolution_clock::now();
        //std::cout << "MCU Color " << std::get<0>(_foreign.objects) << std::endl;
        _thread = SOS::Behavior::Loop::start(this);
    }
    virtual ~MCU() final
    {
        //Interface
        while (!exit_query())
            std::this_thread::yield();
        std::cout << typeid(*this).name() << " shutdown" << std::endl;
        SOS::Behavior::Loop::destroy(_thread);
        //Debug
        kill_time = std::chrono::high_resolution_clock::now();
        //std::cout << "MCU Color " << std::get<0>(_foreign.objects) << std::endl;
        std::cout << "Dumping MCU DMA Objects" << std::endl;
        dump_descriptors_binary(this->descriptors, rx_counter, tx_counter, boot_time, kill_time);
        if (SOS::Protocol::Serial<MCUProcessingSwitch, TrueColorClass, DMA, DMA>::reads_pending())
            SFA::util::runtime_error(SFA::util::error_code::ReadsPendingAfterComthreadDestruction, __FILE__, __func__, typeid(*this).name());
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
        if (_vars.received_com_shutdown && !transfers_pending() && !_vars.acknowledgeRequested && !_vars.received_acknowledge && _vars.descendants_notified)
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
    std::thread _thread;
};
