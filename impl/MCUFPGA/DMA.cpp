typedef unsigned char DMA[252]; // 8bit: max, 252%3==0
std::ostream& operator<<(std::ostream& os, const DMA& c) // avoid missing C string termination
{
    //os << std::hex;
    for (std::size_t j = 0; j < sizeof(c); j++) {
        // printf("%X", reinterpret_cast<unsigned char*>(c)[j] );
        //os << reinterpret_cast<unsigned char*>(&c)[j];
        os << c[j];
    }
    //os << std::dec;
    return os;
};