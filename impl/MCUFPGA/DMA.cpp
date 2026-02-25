//#define DMA std::array<unsigned char,999>//1001%3=2
struct DMA : std::array<unsigned char,999> {};//1001%3=2
std::ostream& operator<<(std::ostream& os, DMA c) {
    os << std::hex;
    for (std::size_t j=0;j<sizeof(c);j++){
        //printf("%X", reinterpret_cast<unsigned char*>(c)[j] );
        os << reinterpret_cast<unsigned char*>(&c)[j];
    }
    os << std::dec;
    return os;
};
