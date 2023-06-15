#include <atomic>
#include <array>
#include <tuple>
#include <initializer_list>
#include <iostream>

template<typename... T> struct Cable : public std::tuple< std::atomic<T>... >{
    using std::tuple< std::atomic<T>... >::tuple;
};

template<typename ArithmeticType> struct RingBufferTask2Cable : public Cable<
ArithmeticType, ArithmeticType
>{
    //using Cable< ArithmeticType, ArithmeticType >::Cable;
    RingBufferTask2Cable(ArithmeticType current,ArithmeticType threadcurrent, const ArithmeticType end) :
    Cable< ArithmeticType, ArithmeticType > {current,threadcurrent},
    end(end)
    {

    }
    const ArithmeticType end;
};

template<typename ArithmeticType> struct RingBufferTask1Cable {
    std::atomic<std::size_t> Current;
    std::atomic<std::size_t> ThreadCurrent;
    std::atomic<std::array<ArithmeticType,1000>*> SingleBuffer;
};

class RingBufferBus {
    public:
    using arithmetic_type = std::array<double,1000>::iterator;
    RingBufferBus() :
    buffer(std::array<double,1000>{}),
    task2(RingBufferTask2Cable<arithmetic_type>{buffer.begin(),buffer.begin(),buffer.end()})
    {}
    RingBufferTask2Cable<arithmetic_type> task2;
    private:
    std::array<double,1000> buffer;
};

template<typename ArithmeticType> class RingBufferTask1 {
    public:
    static const RingBufferTask1Cable<ArithmeticType> CableFactory(std::size_t current, std::size_t threadcurrent){
        return RingBufferTask1Cable<ArithmeticType>{
            std::atomic<std::size_t>{current},
            std::atomic<std::size_t>{threadcurrent}
            ,std::atomic<std::array<ArithmeticType,1000>*>{ new std::array<ArithmeticType,1000>{}}
            };
    }
};

int main(){
    auto array = std::array<double,1000>{};
    auto tuple = std::tuple<std::array<double,1000>*,std::array<double,1000>*>{(std::array<double,1000>*)array.begin(),(std::array<double,1000>*)array.begin()};
    auto cable = Cable<std::array<double,1000>*,std::array<double,1000>*>{(std::array<double,1000>*)array.begin(),(std::array<double,1000>*)array.begin()};
    using arithmetic_type = std::array<double,1000>*;
    //auto task_cable = RingBufferTask2Cable<arithmetic_type>{array.begin(),array.begin(),array.end()};
    auto bus = RingBufferBus{};
    std::cout << "Task2 Current is lock_free "<< std::get<0>(bus.task2).is_lock_free()<<" at "<<std::get<0>(bus.task2).load()<<std::endl;
    std::cout << "Task2 ThreadCurrent is lock_free "<< std::get<1>(bus.task2).is_lock_free()<<" at "<<std::get<1>(bus.task2).load()<<std::endl;
    std::cout << "Task2 end is not atomic "<< bus.task2.end <<" at "<<bus.task2.end<<std::endl;

}