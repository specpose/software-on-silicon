//https://en.cppreference.com/w/cpp/utility/integer_sequence.html
namespace detail {
    template<class T, T I, T N, T... integers>
    struct make_integer_sequence_helper
    {
        using type = typename make_integer_sequence_helper<T, I + 1, N, integers..., I>::type;
    };

    template<class T, T N, T... integers>
    struct make_integer_sequence_helper<T, N, N, integers...>
    {
        using type = std::integer_sequence<T, integers...>;
    };
}
template<class T, T N> using make_integer_sequence = typename detail::make_integer_sequence_helper<T, 0, N>::type;
//https://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
template<typename Function, typename ArgsTuple, std::size_t... I> auto apply(Function& func, ArgsTuple&& args, std::integer_sequence<std::size_t,I...>) {
    return func(std::get<I>(args)...);
}
template<typename Function, typename ArgsTuple> auto apply(Function& func, ArgsTuple& args) {
    auto _args = std::forward_as_tuple(args);
    return apply(func, args, make_integer_sequence<std::size_t, std::tuple_size<ArgsTuple>::value>{} );
}

#include <type_traits>
/*template<typename First, typename... Others> struct capture_types {
    //capture_types() {}
    //template<typename... Types> capture_types(std::tuple<Types...>& other) {}
    using type = (First, Others...);
};*/
//Stroustrup 28.6.4
template<typename... Objects> class Tuple;
template<> class Tuple<> {};//DANGER
template<typename First, typename... Others> class Tuple<First, Others...> : public Tuple<Others...> {
    typedef Tuple<Others...> inherited;
public:
    constexpr Tuple(){}
    Tuple(First& h, Others&... t)
    : counter(sizeof...(Others)), m_head{h}, inherited(t...) {}
    template<typename... Objects> Tuple(const Tuple<Objects...>& other)
    : m_head(other.head()), inherited(other.tail()) {}
    template<typename... Objects> Tuple& operator=(const Tuple<Objects...>& other){
        m_head=other.head();
        tail()=other.tail();
        return *this;
    }
    //private:
    First& head(){return m_head;};
    const First& head() const {return m_head;};
    inherited& tail(){return *this;};
    const inherited& tail() const {return *this;};
    std::size_t counter;
protected:
    First m_head;
};
template<unsigned char N, typename F, typename... T>
auto& get(Tuple<F,T...>& t) {
    if constexpr(N>0){
        return get<N-1>(t.tail());
    } else {
        return t.head();
    }
}