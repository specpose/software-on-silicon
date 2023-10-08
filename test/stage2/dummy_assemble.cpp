#include "software-on-silicon/EventLoop.hpp"
#define DMA std::array<unsigned char,std::numeric_limits<unsigned char>::max()-2>
DMA com_buffer;
#include "software-on-silicon/Serial.hpp"
#include <iostream>

int main (){
    SOS::Protocol::Serial input;
    unsigned char one = 42;
    printf("%d\n",one);
    printf("%c\n",one);
    unsigned char two = 95;
    printf("%d\n",two);
    printf("%c\n",two);
    unsigned char three = 63;
    printf("%d\n",three);
    printf("%c\n",three);
    printf("%d\n",0xFF);
    printf("%c\n",0xFF);
    std::cout<<std::bitset<8>{static_cast<unsigned long>(one)}<<std::endl;
    std::bitset<8> out = input.write_assemble(one);
    input.writeCount++;
    com_buffer[0]=static_cast<unsigned char>(out.to_ulong());
    std::cout<<std::bitset<8>{static_cast<unsigned long>(two)}<<std::endl;
    out.reset();
    out = input.write_assemble(two);
    out = input.write_recover(out);
    input.writeCount++;
    com_buffer[1]=static_cast<unsigned char>(out.to_ulong());
    std::cout<<std::bitset<8>{static_cast<unsigned long>(three)}<<std::endl;
    out.reset();
    out = input.write_assemble(three);
    out = input.write_recover(out);
    input.writeCount++;
    com_buffer[2]=static_cast<unsigned char>(out.to_ulong());
    std::cout<<std::bitset<8>{static_cast<unsigned long>(0xFF)}<<std::endl;
    out.reset();
    out = input.write_assemble(0xFF,false);
    out = input.write_recover(out);
    com_buffer[3]=static_cast<unsigned char>(out.to_ulong());
    for(int i=0;i<5;i++){
        printf("%d\n",com_buffer[i]);
    }
    SOS::Protocol::Serial output;
    output.read(com_buffer[0]);
    std::cout<<output.in<<std::endl;
    output.read(com_buffer[1]);
    std::cout<<output.in<<std::endl;
    output.read(com_buffer[2]);
    std::cout<<output.in<<std::endl;
    output.read(com_buffer[3]);
    std::cout<<output.in<<std::endl;
    auto result = output.read_flush();
    for(int i=0;i<3;i++){
        printf("%d\n",result[i]);
        printf("%c\n",result[i]);
    }
}