struct TrueColor {
    TrueColor(){
        std::get<0>(RGB) = 0;
        std::get<1>(RGB) = 0;
        std::get<2>(RGB) = 0;
    }
    friend std::ostream& operator<<(std::ostream& os, TrueColor& c)
    {
        return os << "r: 0x" << std::hex << static_cast<int>(c.red()) << " g: 0x" << std::hex << static_cast<int>(c.green()) << " b: 0x" << std::hex << static_cast<int>(c.blue()) << std::dec;
    }
    unsigned char& red() { return std::get<0>(RGB); }
    unsigned char& green() { return std::get<1>(RGB); }
    unsigned char& blue() { return std::get<2>(RGB); }
    std::array<unsigned char,3> RGB{ 0, 0, 0 };
};