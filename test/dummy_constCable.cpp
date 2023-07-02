#include <array>
#include <tuple>
#include <atomic>

//cables(
//forward_as_tuple only works with REFERENCES of rvalues
//tie needs referenceable (=lvalue)
//std::tie(ReadLength<_pointer_type>{},ReadOffset<_difference_type>{} )
//tuple needs storage?
//std::make_tuple(ReadLength<_pointer_type>(), ReadOffset<_difference_type>() )
//std::make_tuple(ReadLength<_pointer_type>{}, ReadOffset<_difference_type>{})
//std::tuple< ReadLength<_pointer_type>,ReadOffset<_difference_type> >{{},{}}
//tuple is using either default or reference constructor. No copy, no move?
//std::tuple< ReadLength<_pointer_type>,ReadOffset<_difference_type> >{std::move(ReadLength<_pointer_type>{}),std::move(ReadOffset<_difference_type>{})}
//std::tuple< ReadLength<_pointer_type>,ReadOffset<_difference_type> >{ReadLength<_pointer_type>{},ReadOffset<_difference_type>{}}
//std::tuple< ReadOffset<_difference_type> >{}
//),

template<typename T, std::size_t N> struct ConstCable : public std::array<const T,N>{
    using wire_names = enum class empty : unsigned char{} ;
    using cable_arithmetic = T;
    //using std::array<const T,N>::array;
};
template<typename ArithmeticType> struct ReadLength : public ConstCable<ArithmeticType,2> {
    //using ConstCable<ArithmeticType, 2>::ConstCable;
    enum class wire_names : unsigned char { startPos, afterLast };
};

template<typename T, std::size_t N> struct TaskCable : public std::array<std::atomic<T>,N>{
    using wire_names = enum class empty : unsigned char{} ;
    using cable_arithmetic = T;
};

class AObject {
    public:
    AObject () {}//= delete;
    AObject(const AObject& copyme) {}//= delete;
};

int main() {
    auto hostmemory = std::array<char,10>{};
    auto myCable = ReadLength<decltype(hostmemory)::iterator>{hostmemory.begin(),hostmemory.end()};
    //auto myCables = std::tuple< decltype(myCable) >( {hostmemory.begin(),hostmemory.end()} );
    struct Bus {
        std::tuple< AObject > acable;
        AObject obj;
        Bus(decltype(hostmemory)::iterator s, decltype(hostmemory)::iterator e)
        : acable(std::tuple< AObject >( AObject{} ))
        {}
    };
    auto myBus=Bus(hostmemory.begin(),hostmemory.end());
}