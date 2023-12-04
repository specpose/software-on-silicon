#include <iostream>
#include "software-on-silicon/EventLoop.hpp"
#include <limits>
#include "software-on-silicon/error.hpp"
#include "software-on-silicon/serial_helpers.hpp"
struct MaximalSymbolRate {
    MaximalSymbolRate(){
        unsigned long number = 8388607;
        std::bitset<24> combined_bits{number};
        combined_bits[23]=true;//mcu_owned        
        //StatusAndNumber = reinterpret_cast<decltype(StatusAndNumber)>(combined_bits.to_ullong());
        /*auto byteZero = ((combined_bits << (3-3)*8) >> (3-1)*8);
        StatusAndNumber[0] = static_cast<unsigned char>(byteZero.to_ulong());//big end
        auto byteOne =((combined_bits << (3-2)*8) >> (3-1)*8);
        StatusAndNumber[1] = static_cast<unsigned char>(byteOne.to_ulong());
        auto byteTwo = ((combined_bits << (3-1)*8) >> (3-1)*8);
        StatusAndNumber[2] = static_cast<unsigned char>(byteTwo.to_ulong());//little endian*/
        bitsetToBytearray<sizeof(StatusAndNumber)>(StatusAndNumber,combined_bits);
        //std::cout<<"bytes: "<<byteZero.to_string()<<", "<<byteOne.to_string()<<", "<<byteTwo.to_string()<<std::endl;
    }
    bool mcu_owned(){
        std::bitset<8> first_byte{static_cast<unsigned int>(StatusAndNumber[0])};
        first_byte = (first_byte >> 7) << 7;
        bool result = first_byte[7];//bigend => numbering is right to left, Stroustrup 34.2.2
        return result;
    }
    unsigned int getNumber(){
        std::bitset<24> combined_bits;
        bytearrayToBitset<3>(combined_bits,StatusAndNumber);
        auto stripped = ((combined_bits << 1) >> 1);
        return stripped.to_ulong();
    }
    unsigned char StatusAndNumber[3];//23 bits number: unsigned maxInt 8388607
};
int main () {
    unsigned int maxInt = 0;
    auto a = MaximalSymbolRate();
    std::cout<<"Size of MaximalSymbolRate "<<sizeof(a)<<std::endl;
    std::cout<<"mcu_owned: "<<a.mcu_owned()<<std::endl;
    std::cout<<"number: "<<a.getNumber()<<std::endl;
}