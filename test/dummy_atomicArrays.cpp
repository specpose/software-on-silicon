#include <array>
#include <atomic>
#include <tuple>

template<typename T, std::size_t N> struct Cable : public std::array<std::atomic<T>,N>{

};

template<typename T, std::size_t N> struct TaskCable : Cable<T,2>{
    //using std::array<std::atomic<T>,N>::array;
    using wire_names = enum class empty : unsigned char{} ;
    using cable_arithmetic = T;
    TaskCable(T current, T threadcurrent) : Cable<T,2>{current, threadcurrent} {}
};

int main(){
    auto t1 = std::array<std::atomic<int>,2>{0,0};
    auto t2 = TaskCable<int,2>{0,0};
    auto t3 = std::tuple< std::array<int,2> >{ std::array<int,2>{0,0} };
    static_assert(std::is_constructible<int>::value==true);
    static_assert(std::is_constructible<std::atomic<int>>::value==true);
    static_assert(std::is_constructible<std::array<std::atomic<int>,2>>::value==true);
    static_assert(std::is_constructible<std::tuple< std::array<std::atomic<int>,2> >>::value==true);
    auto a1 = std::atomic<int>{0};
    auto a2 = std::array<std::atomic<int>,2>{0,0};
    //tuple constructor uses move or assignment operator ?
    auto a3 = std::tuple< std::array<int,2> >{ std::array<int,2>{0,0} };
    //tuple requires copy constructor for any tuple that isn't default constructed
    auto a4 = std::tuple< std::array<std::atomic<int>,2> >{ };
    //not is_explicitly_constructible, is_implicitly_constructible, is_explicitly_default_constructible, is_implicitly_default_constructible
    //auto t4 = std::tuple< std::array<std::atomic<int>,2> >{ std::array<std::atomic<int>,2>{0,0} };
    //auto t4 = std::tuple< TaskCable<int,2> >{ };
}