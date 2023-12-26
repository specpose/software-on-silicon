#include <iostream>
#include "software-on-silicon/INTERFACE.hpp"
#include <limits>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/Serial.hpp"

using namespace SOS::Protocol;

struct SymbolRateCounter {
    SymbolRateCounter(){
        setNumber(0);
        set_mcu_owned();
    }
    bool mcu_owned(){
        std::bitset<8> first_byte{static_cast<unsigned int>(StatusAndNumber[0])};
        first_byte = (first_byte >> 7) << 7;
        bool result = first_byte[7];//bigend => numbering is right to left, Stroustrup 34.2.2
        return result;
    }
    void set_mcu_owned(bool state=true){
        std::bitset<8> save_first_byte{static_cast<unsigned int>(StatusAndNumber[0])};
        save_first_byte[7]= state;
        StatusAndNumber[0]=static_cast<decltype(StatusAndNumber)::value_type>(save_first_byte.to_ulong());
    }
    unsigned int getNumber(){
        std::bitset<24> combined_bits;
        bytearrayToBitset(combined_bits,StatusAndNumber);
        auto stripped = ((combined_bits << 1) >> 1);
        return stripped.to_ulong();
    }
    void setNumber(unsigned int number){
        bool save_ownership = mcu_owned();
        std::bitset<24> combined_bits{number};
        combined_bits[23]=save_ownership;//mcu_owned        
        bitsetToBytearray(StatusAndNumber,combined_bits);
    }
    std::array<unsigned char,3> StatusAndNumber;//23 bits number: unsigned maxInt 8388607
};
int main () {
    std::cout<<std::numeric_limits<std::bitset<23>>::max()<<std::endl;
    unsigned int maxInt = 0;
    auto a = SymbolRateCounter();
    a.setNumber(8388607);
    std::cout<<"Size of SymbolRateCounter "<<sizeof(a)<<std::endl;
    std::cout<<"mcu_owned: "<<a.mcu_owned()<<std::endl;
    std::cout<<"number: "<<a.getNumber()<<std::endl;
    a.set_mcu_owned(false);
    std::cout<<"mcu_owned before setNumber: "<<a.mcu_owned()<<std::endl;
    a.setNumber(8388606);
    std::cout<<"mcu_owned after setNumber: "<<a.mcu_owned()<<std::endl;
    std::cout<<"number: "<<a.getNumber()<<std::endl;
}