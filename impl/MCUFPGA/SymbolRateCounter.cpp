struct SymbolRateCounter {
    signed char red;
    unsigned char green;
    unsigned char blue;
};
struct SymbolRateCounterClass : SymbolRateCounter {
    SymbolRateCounterClass() : SymbolRateCounter( { 0, 0, 0 } ) {}
    SymbolRateCounter& operator++() {
        if ( blue < 255 ) {
            ++blue;
        } else {
            if ( green < 255 ) {
                blue = 0;
                ++green;
            } else {
                if ( red < 128 ) {
                    blue = 0;
                    green = 0;
                    ++red;
                } else {
                    SFA::util::runtime_error(SFA::util::error_code::NaN, __FILE__, __func__, typeid(*this).name());
                }
            }
        }
        return *this;
    }
    int to_int32() {
        return (red*255*255)+(green*255)+blue;
    }
    friend std::ostream& operator<<(std::ostream& os, SymbolRateCounterClass& c) {
        os << c.to_int32();
    }
};