namespace SOS {
namespace Protocol {
    template <typename... Objects>
    BlockWiseTransfer<Objects...>::BlockWiseTransfer(std::tuple<Objects...>& objects)
    {
        this->descriptors(objects, make_integer_sequence<std::size_t, std::tuple_size<std::tuple<Objects...>>::value> {}); // integer_sequence: cpp14
        // apply(this->descriptors, objects); // fold expression: cpp17
    }
    /*
        template <> BlockWiseTransfer<TrueColor, DMA, DMA>::BlockWiseTransfer(std::tuple<TrueColor, DMA, DMA>& objects)
            //: descriptors(
            //{ { static_cast<unsigned char>(0), reinterpret_cast<void*>(&std::get<0>(objects)), sizeof(std::get<0>(objects)), false, false, false },
            //{ static_cast<unsigned char>(1), reinterpret_cast<void*>(&std::get<1>(objects)), sizeof(std::get<1>(objects)), false, false, false },
            //{ static_cast<unsigned char>(2), reinterpret_cast<void*>(&std::get<2>(objects)), sizeof(std::get<2>(objects)), false, false, false } },
            //3
            //)
        {
            this->descriptors = { { { static_cast<unsigned char>(0), reinterpret_cast<void*>(&std::get<0>(objects)), sizeof(std::get<0>(objects)), false, false, false },
            { static_cast<unsigned char>(1), reinterpret_cast<void*>(&std::get<1>(objects)), sizeof(std::get<1>(objects)), false, false, false },
            { static_cast<unsigned char>(2), reinterpret_cast<void*>(&std::get<2>(objects)), sizeof(std::get<2>(objects)), false, false, false } },
            3};
            std::cout<<"Initialised Descriptors"<<std::endl;
            for (std::size_t i = 0; i < this->descriptors.size(); i++) {
                std::cout << "DMAObject " << i << " ptr: " << (this->descriptors)[i].obj << " size: " << (this->descriptors)[i].obj_size << std::endl;
            }
        }
    */
}
}