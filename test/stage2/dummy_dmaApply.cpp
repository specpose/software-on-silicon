#include <tuple>
#include <array>
#include <iostream>
#include <limits>
struct DMADescriptor {
    DMADescriptor(){}//DANGER
    DMADescriptor(unsigned char id, void* obj, std::size_t obj_size) : id(id),obj(obj),obj_size(obj_size){}
    unsigned char id = 0xFF;
    void* obj = nullptr;
    std::size_t obj_size = 0;
    bool synced = true;
};
template<unsigned int N> struct DescriptorHelper : public std::array<DMADescriptor,N> {
    public:
    using std::array<DMADescriptor,N>::array;
    template<typename... T> void operator()(T&... obj_ref){
        (assign(obj_ref),...);
    }
    private:
    template<typename T> void assign(T& obj_ref){
        (*this)[count] = DMADescriptor(static_cast<unsigned char>(count),reinterpret_cast<void*>(&obj_ref),sizeof(obj_ref));
        count++;
    }
    std::size_t count = 0;
};
int main () {
    using objects_type = std::tuple<bool,int,float>;
    objects_type objects{};
    std::get<0>(objects)=true;
    std::get<1>(objects)=-std::numeric_limits<int>::max();
    std::get<2>(objects)=-std::numeric_limits<float>::min();
    std::cout<<(void*)&std::get<0>(objects)<<", "<<(void*)&std::get<1>(objects)<<", "<<(void*)&std::get<2>(objects)<<std::endl;
    DescriptorHelper<std::tuple_size<objects_type>::value> descriptors{};
    std::apply(descriptors,objects);//ALWAYS: Initialize Descriptors in Constructor
    for (int i=0;i<descriptors.size();i++){
        std::cout<<"Descriptor id "<<static_cast<unsigned int>(descriptors[i].id)<<" has obj address "<<descriptors[i].obj;
        auto test = reinterpret_cast<unsigned char*>(descriptors[i].obj);
        auto test_size = descriptors[i].obj_size;
        std::cout<<" and size "<<test_size<<std::endl;
        for (unsigned int j=0;j<test_size;j++)
            std::cout<<"byte no. "<<j<<" of object is "<<static_cast<unsigned int>(test[j])<<std::endl;
    }
}