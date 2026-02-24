template<unsigned char N, typename F, typename... T>
auto& get(Tuple<F,T...>& t) {
    if constexpr(N>0){
        return get<N-1>(t.tail());
    } else {
        return t.head();
    }
}