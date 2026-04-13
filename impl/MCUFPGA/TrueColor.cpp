struct TrueColor {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};
struct TrueColorClass : TrueColor {
    TrueColorClass() : TrueColor( { 0, 0, 0} ) {}
    friend std::ostream& operator<<(std::ostream& os, const TrueColorClass& c)
    {
        return os << "r: 0x" << std::hex << static_cast<int>(c.red) << " g: 0x" << std::hex << static_cast<int>(c.green) << " b: 0x" << std::hex << static_cast<int>(c.blue) << std::dec;
    }
};