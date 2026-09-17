namespace SOS {
namespace MemoryView {
    template <typename ComIterator>
    struct ComSize : public SOS::MemoryView::ConstCable<ComIterator, 4> {
        using SOS::MemoryView::ConstCable<ComIterator, 4>::ConstCable;
        typename SOS::MemoryView::ConstCable<ComIterator, 4>::value_type& getInBufferStartRef() { return std::get<0>(*this); }
        typename SOS::MemoryView::ConstCable<ComIterator, 4>::value_type& getInBufferEndRef() { return std::get<1>(*this); }
        typename SOS::MemoryView::ConstCable<ComIterator, 4>::value_type& getOutBufferStartRef() { return std::get<2>(*this); }
        typename SOS::MemoryView::ConstCable<ComIterator, 4>::value_type& getOutBufferEndRef() { return std::get<3>(*this); }
    };
    template <typename ComIterator>
    struct ComOffset : public SOS::MemoryView::TaskCable<ComIterator, 2> {
        using SOS::MemoryView::TaskCable<ComIterator, 2>::TaskCable;
        typename SOS::MemoryView::TaskCable<ComIterator, 2>::value_type& getReadOffsetRef() { return std::get<0>(*this); }
        typename SOS::MemoryView::TaskCable<ComIterator, 2>::value_type& getWriteOffsetRef() { return std::get<1>(*this); }
    };
    template <typename ComBufferType>
    struct ComBus : public bus<
                        bus_double_shaker_tag,
                        SOS::MemoryView::DoubleHandShake,
                        bus_traits<Bus>::cables_type,
                        bus_traits<Bus>::const_cables_type> {
        signal_type signal;
        using const_cables_type = std::tuple<ComSize<typename ComBufferType::iterator>>;
        using cables_type = std::tuple<ComOffset<typename ComBufferType::difference_type>>;
        ComBus(const typename ComBufferType::iterator& inStart, const typename ComBufferType::iterator& inEnd, const typename ComBufferType::iterator& outStart, const typename ComBufferType::iterator& outEnd)
            : const_cables { ComSize<typename ComBufferType::iterator>({ inStart, inEnd, outStart, outEnd }) }
        {
            std::get<0>(cables).getReadOffsetRef() = 0;
            std::get<0>(cables).getWriteOffsetRef() = 0;
            if (std::distance(inStart, inEnd) < 1)
                SFA::util::logic_error(SFA::util::error_code::CombufferSizeIsMinimumWORDSIZE, __FILE__, __func__, typeid(*this).name());
            if (std::distance(outStart, outEnd) < 1)
                SFA::util::logic_error(SFA::util::error_code::CombufferSizeIsMinimumWORDSIZE, __FILE__, __func__, typeid(*this).name());
            if (std::distance(inStart, inEnd) != std::distance(outStart, outEnd))
                SFA::util::logic_error(SFA::util::error_code::CombufferInAndOutSizeNotEqual, __FILE__, __func__, typeid(*this).name());
        }
        cables_type cables {};
        const_cables_type const_cables;
    };
}
}
