// https://en.cppreference.com/w/cpp/utility/integer_sequence.html
namespace detail {
template <class T, T I, T N, T... integers>
struct make_integer_sequence_helper {
    using type = typename make_integer_sequence_helper<T, I + 1, N, integers..., I>::type;
};

template <class T, T N, T... integers>
struct make_integer_sequence_helper<T, N, N, integers...> {
    using type = std::integer_sequence<T, integers...>;
};
}
template <class T, T N>
using make_integer_sequence = typename detail::make_integer_sequence_helper<T, 0, N>::type;
// https://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
template <typename Function, typename ArgsTuple, std::size_t... I>
auto apply(Function& func, ArgsTuple&& args, std::integer_sequence<std::size_t, I...>)
{
    return func(std::get<I>(args)...);
}
template <typename Function, typename ArgsTuple>
auto apply(Function& func, ArgsTuple& args)
{
    auto _args = std::forward_as_tuple(args);
    return apply(func, args, make_integer_sequence<std::size_t, std::tuple_size<ArgsTuple>::value> {});
}