#include <iostream>
#include <type_traits>

#include "cpp11.hpp"

template<typename T>
T& get(Tuple<T>& t) {
    return t.head();
}
template<typename First, typename Second>
auto get(Tuple<First,Second>& t) ->First& {// polymorphism?
    return get<First>(t);// => 1
}
template<typename First, typename Second>
auto get(Tuple<First,Second>& t) ->Second& {// polymorphism?
    return get<Second>(t.tail());// => 1
}
template<std::size_t N, typename T>
T& get(Tuple<T, T>& t) {
    if (N==1)
        return get<N-1, T>(t.tail());// ?
    return get<N, T, T>(t);// => 9
}
template<std::size_t N, typename T>
T& get(Tuple<T>& t) {
    if (N==1)
        //return get<N-1>(t.tail());// oxymoron
        return get<N-1,T>(t.tail());
    return get<T>(t);// => 1
}
template<std::size_t N, typename First, typename Second>
First& get(Tuple<First, Second>& t) {
    if (N==1)
        return get<N-1, First>(t);
    return get<First>(t);// => 1
}
template<std::size_t N, typename First, typename Second>
Second& get(Tuple<First, Second>& t) {
    if (N==1)
        return get<N-1, Second>(t.tail());
    return get<Second>(t);// => 1
}
template<std::size_t N, typename T, typename... Types>
T& get(Tuple<T, T, Types...>& t) {
    if (sizeof...(Types)+1>N)
        return get<N-1, T, Types...>(t.tail());
    return get<N, T, T, Types...>(t);
}
/*template<std::size_t N, typename T, typename... Types>
T& get(Tuple<T, Types...>& t) {
    if (sizeof...(Types)>N)
        return get<N-1, Types...>(t.tail());// not allowed
    return get<N, T, Types...>(t);
}*/
template<std::size_t N, typename First, typename Second, typename... Types>
First& get(Tuple<First, Second, Types...>& t) {
    if (sizeof...(Types)+1>N)
        return get<N-1, First, Second, Types...>(t);
    return get<N,First, Second, Types...>(t);
}
template<std::size_t N, typename First, typename Second, typename... Types>
Second& get(Tuple<First, Second, Types...>& t) {
    if (sizeof...(Types)+1>N)
        return get<N-1, Second, Types...>(t.tail());
    return get<N, Second, Types...>(t.tail());
}

int main() {
    Tuple<bool,int,float> objects = Tuple<bool,int,float>{};
    bool one = true;
    int two = 5;
    float three = 7.7;
    objects = Tuple<bool,int,float>(one,two,three);
    std::cout<<std::endl;
    std::cout<<static_cast<unsigned int>(get<0>(objects))<<std::endl;
    std::cout<<static_cast<unsigned int>(get<1>(objects))<<std::endl;
    std::cout<<static_cast<unsigned int>(get<2>(objects))<<std::endl;
}