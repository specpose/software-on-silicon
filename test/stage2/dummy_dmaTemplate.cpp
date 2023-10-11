#include <iostream>
#include <type_traits>

struct DMADescriptor {
    DMADescriptor(){}//DANGER
    DMADescriptor(unsigned char id, void* obj) : id(id),obj(obj){}
    unsigned char id;
    void* obj;
};

//Stroustrup 28.6.4
template<typename... Objects> class DMADescriptors;

template<> class DMADescriptors<> {};//DANGER

template<typename First, typename... Others> class DMADescriptors<First, Others...> : private DMADescriptors<Others...> {
    typedef DMADescriptors<Others...> inherited;
    public:
    constexpr DMADescriptors(){}
    DMADescriptors(First h, Others... t)
    : m_head(sizeof...(Others),nullptr), inherited(t...) {}
    template<typename... Objects> DMADescriptors(const DMADescriptors<Objects...>& other)
    : m_head(other.head()), inherited(other.tail()) {}
    template<typename... Objects> DMADescriptors& operator=(const DMADescriptors<Objects...>& other){
        m_head=other.head();
        tail()=other.tail();
        return *this;
    }
    //private:
    DMADescriptor& head(){return m_head;};
    const DMADescriptor& head() const {return m_head;};
    inherited& tail(){return *this;};
    const inherited& tail() const {return *this;};
    protected:
    DMADescriptor m_head;
};

template<unsigned char N, typename... T>
DMADescriptor& get(DMADescriptors<T...>& t) {
    if constexpr(sizeof...(T)!=0){
        if constexpr(N < sizeof...(T)){
            //if ((t.head().id-(sizeof...(T)-1))==id){
            if ((t.head().id)==N){
                return t.head();
            }
            return get<N>(t.tail());
        } else {
            throw std::runtime_error("DMADescriptor does not exist");
        }
    } else {
        throw std::logic_error("get<> can not be used with DMADescriptors<>");
    }
}

template<typename... Objects> constexpr std::size_t DMADescriptors_size(){ return sizeof...(Objects); }

//https://stackoverflow.com/questions/1198260/how-can-you-iterate-over-the-elements-of-an-stdtuple
template<typename... T>
DMADescriptor& get(DMADescriptors<T...>& t,unsigned char id) {
    if constexpr(sizeof...(T)!=0){
        if (id < sizeof...(T)){
            //if ((t.head().id-(sizeof...(T)-1))==id){
            if ((t.head().id)==id){
                return t.head();
            }
            return get(t.tail(),id);
        } else {
            throw std::runtime_error("DMADescriptor does not exist");
        }
    } else {
        throw std::logic_error("get<> can not be used with DMADescriptors<>");
    }
}

int main() {
    DMADescriptors<bool,int,float> objects = DMADescriptors<bool,int,float>{};
    objects = DMADescriptors<bool,int,float>(true,5,7.7);
    std::cout<<static_cast<unsigned int>(objects.head().id)<<std::endl;
    std::cout<<static_cast<unsigned int>(objects.tail().head().id)<<std::endl;
    std::cout<<static_cast<unsigned int>(objects.tail().tail().head().id)<<std::endl;
    std::cout<<static_cast<unsigned int>(get<0>(objects).id)<<std::endl;
    std::cout<<static_cast<unsigned int>(get<1>(objects).id)<<std::endl;
    std::cout<<static_cast<unsigned int>(get<2>(objects).id)<<std::endl;
    for(int i=0;i<DMADescriptors_size<bool,int,float>();i++)
        std::cout<<static_cast<unsigned int>(get(objects,i).id)<<std::endl;
}