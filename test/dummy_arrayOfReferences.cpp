#include <cstddef>
#include <array>
#include <atomic>

template<typename T,int N> struct TypedWire {
    TypedWire(std::array<T,N> values){
        for(int i=0;i<N;i++){
            ManyValues[i]=std::atomic<int>{values[i]};
        }
    }
    private:
    std::array<std::atomic<size_t>,N> ManyValues;
};

template<typename ArithmeticType, unsigned int N> struct TypedWire2 : public std::array<std::atomic<ArithmeticType>,N>{
    //TypedWire(std::initializer_list<constexpr ArithmeticType> list) {
    //}
    //TypedWire(std::initializer_list<ArithmeticType> list) {
        /*for(int i=0;i!=list.size();++i){
            references[i]=std::atomic<ArithmeticType>{list.begin()[i]};
        }*/
    //}
    //Can the entire array<atomic<int>> be atomic?
    //TypedWire(std::array<ArithmeticType,N> array) : std::array<std::atomic<ArithmeticType>,N>(std::atomic<ArithmeticType>{array}...) {}
    //TypedWire(constexpr ArithmeticType[N] array) {}/: std::array<std::atomic<ArithmeticType>,N>(std::atomic<ArithmeticType>{array}...) {}
    //private:
    //std::atomic<ArithmeticType>* references[N];
};

template<int Value> void func(){}
template<int... Values> void test(){}
//template<double Value> void func(){}

int main () {
    //auto RingBufferItem = TypedWire<unsigned int,2>{0,1};
    auto values = TypedWire<int,2>{std::array<int,2>{0,1}};
}