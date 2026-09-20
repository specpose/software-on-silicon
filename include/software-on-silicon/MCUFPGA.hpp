namespace SOS {
namespace Behavior {
    template <typename... Objects>
    class FPGACrossover : public SOS::Protocol::Serial<Objects...> {
    public:
        using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
        FPGACrossover(bus_type& myBus, SOS::MemoryView::SerialResolverBus<Objects...>& other)
        : SOS::Protocol::Serial<Objects...>(myBus, other)
        {
        }
        ~FPGACrossover() {
        };

    private:
        virtual void read_bits(std::bitset<8> temp) final
        {
            SOS::Protocol::Serial<Objects...>::mcu_updated = temp[7];
            SOS::Protocol::Serial<Objects...>::fpga_acknowledge = temp[6];
            SOS::Protocol::Serial<Objects...>::mcu_acknowledge = false;
        }
        virtual void write_bits(std::bitset<8>& out) final
        {
            if (SOS::Protocol::Serial<Objects...>::fpga_updated)
                out.set(7, 1);
            else
                out.set(7, 0);
            if (SOS::Protocol::Serial<Objects...>::mcu_acknowledge)
                out.set(6, 1);
            else
                out.set(6, 0);
        }
        virtual void send_acknowledge() final
        {
            if (SOS::Protocol::Serial<Objects...>::mcu_updated) {
                SOS::Protocol::Serial<Objects...>::mcu_acknowledge = true;
            }
        }
        virtual void send_request() final
        {
            SOS::Protocol::Serial<Objects...>::fpga_updated = true;
        }
        virtual std::tuple<bool, bool> receive_signals() final
        {
            std::tuple<bool, bool> result { SOS::Protocol::Serial<Objects...>::mcu_updated, false };
            if (SOS::Protocol::Serial<Objects...>::fpga_acknowledge) {
                SOS::Protocol::Serial<Objects...>::fpga_updated = false;
                std::get<1>(result) = true;
            }
            return result;
        }
    };
    template <typename... Objects>
    class MCUCrossover : public SOS::Protocol::Serial<Objects...> {
    public:
        using bus_type = SOS::MemoryView::ComBus<COM_BUFFER>;
        MCUCrossover(bus_type& myBus, SOS::MemoryView::SerialResolverBus<Objects...>& other)
        : SOS::Protocol::Serial<Objects...>(myBus, other)
        {
        }
        ~MCUCrossover() {
        }

    private:
        virtual void read_bits(std::bitset<8> temp) final
        {
            SOS::Protocol::Serial<Objects...>::fpga_updated = temp[7];
            SOS::Protocol::Serial<Objects...>::mcu_acknowledge = temp[6];
            SOS::Protocol::Serial<Objects...>::fpga_acknowledge = false;
        }
        virtual void write_bits(std::bitset<8>& out) final
        {
            if (SOS::Protocol::Serial<Objects...>::mcu_updated)
                out.set(7, 1);
            else
                out.set(7, 0);
            if (SOS::Protocol::Serial<Objects...>::fpga_acknowledge)
                out.set(6, 1);
            else
                out.set(6, 0);
        }
        virtual void send_acknowledge() final
        {
            if (SOS::Protocol::Serial<Objects...>::fpga_updated) {
                SOS::Protocol::Serial<Objects...>::fpga_acknowledge = true;
            }
        }
        virtual void send_request() final
        {
            SOS::Protocol::Serial<Objects...>::mcu_updated = true;
        }
        virtual std::tuple<bool, bool> receive_signals() final
        {
            std::tuple<bool, bool> result { SOS::Protocol::Serial<Objects...>::fpga_updated, false };
            if (SOS::Protocol::Serial<Objects...>::mcu_acknowledge) {
                SOS::Protocol::Serial<Objects...>::mcu_updated = false;
                std::get<1>(result) = true;
            }
            return result;
        }
    };
}
}
