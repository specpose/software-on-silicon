#include <bitset>
#include <cmath>
template<unsigned long Bytes> constexpr unsigned long long maxUnsignedInt() {
    unsigned long long result = 0;
    for (int i=0;i<Bytes;i++){
        result+= 0xFF*std::pow(256,i);
    }
    return result;
};
template<unsigned long Bytes> constexpr unsigned long long bytearrayToUnsignedNumber(std::array<unsigned char,Bytes>& source){
    unsigned long long result = 0;
    for (int i=0;i<Bytes;i++){
        result+= static_cast<unsigned char>(source[i])*std::pow(256,i);
    }
    return result;
};
template<unsigned long Bytes, unsigned long Bits> constexpr void bitsetToBytearray(std::array<unsigned char,Bytes>& dest,std::bitset<Bits>& source){
    if (source.to_ullong() > maxUnsignedInt<Bytes>())
        throw SFA::util::logic_error("BitSet does not fit into char[]",__FILE__,__func__);
    for (int i=0;i<Bytes;i++)//bitset is little endian, first destination byte is bigend
        dest[i]=static_cast<unsigned char>(((source << (Bytes-(Bytes-i))*8) >> (Bytes-1)*8).to_ulong());;
};
template<unsigned long Bytes, unsigned long Bits> constexpr void bytearrayToBitset(std::bitset<Bits>& dest,std::array<unsigned char,Bytes>& source){
    std::bitset<Bits> allSet;
    allSet.set();
    if (maxUnsignedInt<Bytes>() > allSet.to_ullong())
        throw SFA::util::logic_error("Numeric value of BitSet does not fit into char[]",__FILE__,__func__);
    dest.reset();
    for (int i=0;i<Bytes;i++){
        std::bitset<Bits> digit = static_cast<unsigned int>(source[i])*std::pow(256,(Bytes-1)-i);//char[] is used as little endian, first read byte is bigend
        dest = dest ^ digit;
    }
};