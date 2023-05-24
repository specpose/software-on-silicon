#include <type_traits>
#include <array>
#include <iostream>

namespace SOS {
struct Processor {
    int operator()(){
        while(busy==true)
            for(int t=0;t<10000;t++){}
        done=true;
        std::cout<<"Processor "<<id<<" halted."<<std::endl;
        return 0;
    }
    std::size_t id;
    bool busy = false;
    bool done = true;
    bool* next_busy = nullptr;
    bool* next_done = nullptr;
};
template<typename T, std::size_t N>struct Shuffle : public Processor {
    std::array<T,N>::iterator_type start;
    std::array<T,N>::iterator_type end;
};
constexpr bool greaterThanZero(unsigned int number){
    return number>0;
}
template<
 typename ProcessorImplementation,
 unsigned int NumberOfProcessors = 0,
 typename T = typename std::enable_if<greaterThanZero(NumberOfProcessors),ProcessorImplementation>::type
> struct ProcessorArray {
std::array<T,NumberOfProcessors> processors = std::array<T,NumberOfProcessors>{Processor()};
};
template<
 typename ProcessorImplementation,
 unsigned int NumberOfProcessors = 0
> struct ShuffleArray : public ProcessorArray<ProcessorImplementation, NumberOfProcessors> {
ShuffleArray() {
    auto current = this->processors.begin();
    auto end =  this->processors.end();
    end--;
    for(;current!=end;current++){
        auto next=current;
        next++;
        current->next_busy=&next->busy;
        current->next_done=&next->done;
    }
}
int operator()(){
    for(auto& p: this->processors){
        p.busy=true;
        p.done=false;
        p();
    }
    for(auto& p: this->processors){
        *p.next_busy=false;
    }
    auto terminated = false;
    while(terminated=false){
        terminated = true;
        for(auto& p: this->processors){
            if(p.done==false)
                terminated = false;
        }
    }
    return -1;
}
};
}
