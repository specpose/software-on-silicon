namespace SOS {
namespace Protocol {
    /*
    template <typename... Objects>
    SyncProcessor<Objects...>::SyncProcessor(SOS::MemoryView::SerialResolverBus<Objects...>& bus2)
     : _sBus(bus2)
    {
        this->descriptors(objects, make_integer_sequence<std::size_t, std::tuple_size<std::tuple<Objects...>>::value> {}); // integer_sequence: cpp14
        // apply(this->descriptors, objects); // fold expression: cpp17
    }
    */
    template <> SyncProcessor<TrueColorClass, DMA, DMA>::SyncProcessor(SOS::MemoryView::SerialResolverBus<TrueColorClass, DMA, DMA>& bus2)
     : _sBus(bus2)
    {
        const std::size_t s = 3;
        this->descriptors = { { { reinterpret_cast<void*>(&std::get<0>(objects)), sizeof(std::get<0>(objects)), false, false },
        { reinterpret_cast<void*>(&std::get<1>(objects)), sizeof(std::get<1>(objects)), false, false },
        { reinterpret_cast<void*>(&std::get<2>(objects)), sizeof(std::get<2>(objects)), false, false } },
        s};
        std::cout<<"Initialised Descriptors"<<std::endl;
        for (std::size_t i = 0; i < this->descriptors.size(); i++) {
            std::cout << "DMAObject " << i << " ptr: " << (this->descriptors)[i].obj << " size: " << (this->descriptors)[i].obj_size << std::endl;
        }
    }
}
}