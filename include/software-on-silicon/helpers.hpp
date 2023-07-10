#pragma once

#include "software-on-silicon/RingBuffer.hpp"
#include <chrono>
#include <iostream>

using namespace std::chrono;
using namespace SOS::MemoryView;

//error: non-type template parameters of class type only available with ‘-std=c++20’ or ‘-std=gnu++20’
//template<typename DurationType, typename DurationType::period> class Timer {
template<typename DurationType,
        unsigned int Period,
        typename PeriodType = typename std::enable_if<
            true, typename DurationType::duration
            >::type
        > class Timer : public SOS::Behavior::EventLoop<SOS::Behavior::SubController> {
    public:
    Timer(SOS::MemoryView::BusShaker::signal_type& bussignal) :
    SOS::Behavior::EventLoop<SOS::Behavior::SubController>(bussignal) {
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
        if (!_intrinsic.getUpdatedRef().test_and_set()){
            const auto t_start = high_resolution_clock::now();
            const auto c_start = clock();
            operator()();
            c_counter += clock() - c_start;
            t_counter += high_resolution_clock::now() - t_start;
            runCount++;
            _intrinsic.getAcknowledgeRef().clear();
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
template<typename Piece> class PieceWriter {
    public:
    PieceWriter(SOS::MemoryView::RingBufferBus<Piece>& bus) : myBus(bus) {}
    void writePiece(typename Piece::value_type character, typename Piece::difference_type length){
        auto current = std::get<0>(myBus.cables).getCurrentRef().load();
        const auto start = std::get<0>(myBus.const_cables).getWriterStartRef();
        const auto end = std::get<0>(myBus.const_cables).getWriterEndRef();
        if (length>=std::distance(current,end)+std::distance(start,current)){
            throw SFA::util::runtime_error("Individual write length too big or RingBuffer too small",__FILE__,__func__);
        }
        for (typename Piece::difference_type i= 0; i<length;i++){//Lock-free (host) write length!
        if (current!=std::get<0>(myBus.cables).getThreadCurrentRef().load()){
            std::cout<<"=";
            //write directly to HOSTmemory
            *current=character;
            ++current;
            if (current==std::get<0>(myBus.const_cables).getWriterEndRef())
                current = std::get<0>(myBus.const_cables).getWriterStartRef();
            std::get<0>(myBus.cables).getCurrentRef().store(current);
            myBus.signal.getNotifyRef().clear();
        } else {
            //write last bit
            *current=character;
            //do not advance -> current invalid  
            std::cout<<std::endl;
            throw SFA::util::runtime_error("RingBuffer too slow or not big enough",__FILE__,__func__);
        }
        }
    }
    private:
    SOS::MemoryView::RingBufferBus<Piece>& myBus;
};
