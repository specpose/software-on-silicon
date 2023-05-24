#include <iostream>
#include <cassert>

#include "software-on-silicon/ProcessorArray.hpp"

using namespace SOS;

struct ProcessorImpl : public Processor {};

int main()
{
    auto a=SOS::ShuffleArray<ProcessorImpl,2>();
    int i=0;
    auto wire = a.processors.begin();
    auto end = a.processors.end();
    end--;
    for(;wire!=end;wire++){
        std::cout<<"Wire "<<i<<" busy goes from "<<std::addressof(wire->busy)<<" to "<<std::addressof(wire->next_busy)<<std::endl;
        auto nextwire=wire;
        nextwire++;
        assert(wire->next_busy==&(nextwire)->busy);
        std::cout<<"Wire "<<i<<" done goes from "<<std::addressof(wire->done)<<" to "<<std::addressof(wire->next_done)<<std::endl;
        assert(wire->next_done==&(nextwire)->done);
        i++;
    }
    if (a()==-1)
        std::cout<<"ProcessorArray exited abnormally"<<std::endl;
}