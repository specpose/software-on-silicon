#include <atomic>
#include <tuple>
#include <iostream>
#include "software-on-silicon/RingBuffer.hpp"

template<typename ArithmeticType> struct RingBufferBus {
    using cables_type = std::tuple< SOS::MemoryView::TaskCable<ArithmeticType, ArithmeticType> >;
    RingBufferBus(ArithmeticType start, ArithmeticType end) :
    cables(std::tuple<std::array<double, 1000>::iterator,std::array<double, 1000>::iterator>{start,end})
    {
    }
    cables_type cables;
};

int main() {
    auto int1 = std::atomic<int>{0};
    auto int2 = std::atomic<int>{0};

    auto tuple1 = std::tuple<std::atomic<int>, std::atomic<int>>{0,0};
    auto tuple2 = std::tuple< std::tuple<std::atomic<int>, std::atomic<int>> > { std::tuple<int, int>{0,0} };

    std::cout << "boxed tuple first element is lock_free " << std::get<0>(std::get<0>(tuple2)).is_lock_free() << std::endl;

    /*auto test = std::tuple< std::tuple<std::atomic<int>, std::atomic<int>> >
    { std::tuple<std::atomic<int>, std::atomic<int>> {
        std::atomic<int>{0},std::atomic<int>{0}
    }
    };*/

    auto buffer = std::array<double, 1000>{};
    auto cables = std::tuple< SOS::MemoryView::TaskCable<std::array<double, 1000>::iterator, std::array<double, 1000>::iterator> >
    {std::tuple<std::array<double, 1000>::iterator,std::array<double, 1000>::iterator>{buffer.begin(),buffer.end()}};

    std::cout << "cable one first wire is lock_free " << std::get<0>(std::get<0>(cables)).is_lock_free() << std::endl;

    /*auto cables2 = std::tuple< SOS::MemoryView::RingBufferTaskCable< std::array<double, 1000>::iterator >
    { SOS::MemoryView::TaskCable<std::array<double, 1000>::iterator, std::array<double, 1000>::iterator>{buffer.begin(),buffer.end()} };*/

    //auto bus = RingBufferBus<std::array<double,1000>::iterator>{};
    auto bus = RingBufferBus<std::array<double,1000>::iterator>(buffer.begin(),buffer.end());
    //bus.cables = cables;

    std::cout << "bus one cable one first wire is lock_free " << std::get<0>(std::get<0>(bus.cables)).is_lock_free() << std::endl;

}