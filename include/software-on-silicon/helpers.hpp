#pragma once

#include "software-on-silicon/EventLoop.hpp"
#include <ratio>
#include <chrono>
#include <iostream>

using namespace std::chrono;

namespace SOS {
    namespace MemoryView {
        /*template<size_t Number,typename Signal>static bool get(Signal& mySignals) {
            auto stateQuery = std::get<Number>(mySignals).test_and_set();
            if (!stateQuery)
                std::get<Number>(mySignals).clear();
            return stateQuery;
        }*/
    }
}

//error: non-type template parameters of class type only available with ‘-std=c++20’ or ‘-std=gnu++20’
//template<typename DurationType, typename DurationType::period> class Timer {
template<typename DurationType,
        unsigned int Period,
        typename PeriodType = typename std::enable_if<
            true, typename DurationType::duration
            >::type
        > class Timer : public SOS::Behavior::EventLoop<
        SOS::MemoryView::Bus<SOS::MemoryView::TypedWire<>,SOS::MemoryView::HandShake>
        >{
    public:
    Timer(SOS::Behavior::EventLoop<SOS::MemoryView::Bus<SOS::MemoryView::TypedWire<>,SOS::MemoryView::HandShake>>::bus_type& bus) :
    SOS::Behavior::EventLoop<SOS::MemoryView::Bus<SOS::MemoryView::TypedWire<>,SOS::MemoryView::HandShake>>(bus) {
        _thread = start(this);
    }
    ~Timer(){
        stop_requested = true;
        _thread.join();
        std::cout<<"Timer spent "<<duration_cast<DurationType>(t_counter -
        duration_cast<high_resolution_clock::duration>(DurationType{(runCount * Period)})
        ).count()<<" duration units more than expected."<<std::endl;
        std::cout<<"Timer has been run "<<runCount<<" times."<<std::endl;
        std::cout<<"Timer spent "<<
        duration_cast<nanoseconds>(t_counter-
        runCount * duration_cast<high_resolution_clock::duration>(DurationType{Period})
        ).count()
        <<"ns more on average per "<<Period<<" duration units."<<std::endl;
    }
    void event_loop(){
        while(!stop_requested){
        if (std::get<SOS::MemoryView::HandShake::Status::updated>(_intrinsic.signal).test_and_set()){
            std::get<SOS::MemoryView::HandShake::Status::updated>(_intrinsic.signal).clear();
            const auto t_start = high_resolution_clock::now();
            const auto c_start = clock();
            operator()();
            c_counter += clock() - c_start;
            t_counter += high_resolution_clock::now() - t_start;
            runCount++;
            std::get<SOS::MemoryView::HandShake::Status::ack>(_intrinsic.signal).test_and_set();
        } else {
            std::get<SOS::MemoryView::HandShake::Status::updated>(_intrinsic.signal).clear();
        }
        }
    }
    void constexpr operator()(){
        std::this_thread::sleep_for(DurationType{Period});;
    }
    private:
    int runCount = 0;
    clock_t c_counter = 0;
    high_resolution_clock::duration t_counter = high_resolution_clock::duration{};

    std::thread _thread = std::thread{};
    bool stop_requested = false;
};

