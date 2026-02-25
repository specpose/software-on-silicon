//Stroustrup 28.6.4
template<typename... Objects> class Tuple;
template<typename First> class Tuple<First> {
public:
    constexpr Tuple(){}
    Tuple(First& h)
    : count(0), m_head(h) {}
    Tuple(const Tuple<First>& other)
    : m_head(other.head()) {}
    Tuple& operator=(const Tuple<First>& other){
        m_head=other.head();
        return *this;
    }
    //private:
    First& head(){return m_head;};
    const First& head() const {return m_head;};
    std::size_t count;
    private:
    First m_head;
};
template<typename First, typename... Others> class Tuple<First, Others...> {
    Tuple<Others...> inherited;
public:
    constexpr Tuple(){}
    Tuple(First& h, Others&... t)
    : m_head(h), inherited(t...) {
        count = sizeof...(Others);
    }
    Tuple(const Tuple<First, Others...>& other)
    : m_head(other.head()), inherited(other.tail()) {}
    Tuple& operator=(const Tuple<First, Others...>& other){
        m_head=other.head();
        tail()=other.tail();
        return *this;
    }
    //private:
    First& head(){return m_head;};
    const First& head() const {return m_head;};
    Tuple<Others...>& tail(){return inherited;};
    const Tuple<Others...>& tail() const {return inherited;};
    std::size_t count;
    private:
    First m_head;
};