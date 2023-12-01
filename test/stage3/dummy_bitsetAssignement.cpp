#include <bitset>
#include <iostream>

static std::bitset<8> idleState() {//constexpr
    std::bitset<8> id;
    id.set(7,1);//updated==true
    id.set(6,0);//acknowledge==false
    //set 6bit data to "111111"
    for(std::size_t i = 0; i <= id.size()-1-2; i++){
        id.set(i,1);
    }
    return id;//-> "10111111"
}

int main(){
    std::cout<<idleState()<<std::endl;
    unsigned char test = 63;
    printf("%d\n",test);
    auto maxId = static_cast<unsigned long>(((idleState()<<2)>>2).to_ulong());
    std::cout<<maxId<<std::endl;
    if (test==maxId)
        throw std::logic_error("DMADescriptor id is reserved for the serial line idle state");
}