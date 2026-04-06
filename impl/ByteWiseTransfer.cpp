namespace SOS {
namespace Protocol {
    template <typename... Objects> BlockWiseTransfer<Objects...>::BlockWiseTransfer(std::tuple<Objects...>& objects)
    {
        this->descriptors(objects, make_integer_sequence<std::size_t, std::tuple_size<std::tuple<Objects...>>::value> {});
        //apply(this->descriptors, objects); // fold expression: cpp17
    }
}
}