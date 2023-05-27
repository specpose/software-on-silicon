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
    typename std::array<T,N>::iterator_type start;
    typename std::array<T,N>::iterator_type end;
};
namespace util {
constexpr bool greaterThanZero(unsigned int number){
    return number>0;
}}
template<
 typename ProcessorImplementation,
 unsigned int NumberOfProcessors = 0,
 typename T = typename std::enable_if<util::greaterThanZero(NumberOfProcessors),ProcessorImplementation>::type
> struct ProcessorArray {
std::array<T,NumberOfProcessors> processors = std::array<T,NumberOfProcessors>{};
};
template<
 typename ProcessorImplementation,
 unsigned int NumberOfProcessors = 0
> struct ShuffleArray : public ProcessorArray<ProcessorImplementation, NumberOfProcessors> {
ShuffleArray() {
    for(auto current = this->processors.begin();current!=std::prev(this->processors.end());current++){
        current->next_busy=&std::next(current)->busy;
        current->next_done=&std::next(current)->done;
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
