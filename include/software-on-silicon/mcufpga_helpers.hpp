template<typename... Objects> void dump_objects(std::tuple<Objects...>& objects,
SOS::Protocol::DescriptorHelper<std::tuple_size<std::tuple<Objects...>>::value>& descriptors) {
    for (int i=0;i<sizeof...(Objects);i++){
        std::cout<<"Object "<<i<<std::endl;
        for (std::size_t j=0;j<descriptors[i].obj_size;j++){
            printf("%c",reinterpret_cast<char*>(descriptors[i].obj)[j]);
        }
        std::cout<<std::endl;
    }
};