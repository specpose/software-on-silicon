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

namespace SOS {
namespace Protocol {
    struct DescriptorHelper : public std::array<DMADescriptor, NUM_IDS> {
    public:
        using std::array<DMADescriptor, NUM_IDS>::array;
        template <typename T, std::size_t... I>
        void operator()(T& objects, std::integer_sequence<std::size_t, I...>)
        {
            count = 0;
            assign(std::get<I>(objects)...);
            for (std::size_t i = 0; i < count; i++) {
                std::cout << "DMAObject " << i << " ptr: " << (*this)[i].obj << " size: " << (*this)[i].obj_size << std::endl;
            }
        }
        std::size_t size() { return count; }

    private:
        template <typename First>
        void assign(First& obj_ref)
        {
            (*this)[count] = { static_cast<unsigned char>(count), reinterpret_cast<void*>(&obj_ref), sizeof(obj_ref), false, false, false };
            count++;
        }
        template <typename First, typename... Others>
        void assign(First& obj_ref, Others&... objects)
        {
            (*this)[count] = { static_cast<unsigned char>(count), reinterpret_cast<void*>(&obj_ref), sizeof(obj_ref), false, false, false };
            count++;
            assign(objects...);
        }
        std::size_t count = 0;
    };
}
}