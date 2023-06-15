#include <tuple>
#include <atomic>

template<typename... T> class TypedWires : public std::tuple<T...> {
    public:
    using std::tuple<T...>::tuple;
    TypedWires(T... args) : std::tuple<T...>{args...} {}
};

template<typename... Tasks> class TaskCables : public std::tuple<Tasks...>{
    public:
    using std::tuple<Tasks...>::tuple;
};

//should be private
template<typename MyTask, typename... Others> class Task : public TaskCables<Others...> {
    public:
    Task(TaskCables<MyTask, Others...>& wires) : _intrinsic(std::get<0>(wires)) {}
    protected:
    MyTask& _intrinsic;
};

template<unsigned int N> class LastTask {
    public:
    LastTask(TaskCables< TypedWires<std::atomic<float>>,TypedWires<std::atomic<int>>,TypedWires<std::atomic<double>> >& All)
    /*: _taskCable(std::get<
    2
    >(All))*/
    {}
    private:
    //TypedWires<double>& _taskCable;
};
template<unsigned int N> class MiddleTask : public LastTask<N-1> {
    public:
    MiddleTask(TaskCables< TypedWires<std::atomic<float>>,TypedWires<std::atomic<int>>,TypedWires<std::atomic<double>> >& All) : LastTask<N-1>(All)
    /*, _taskCable(std::get<
    1
    >(All))*/
    {}
    private:
    //TypedWires<int>& _taskCable;
};
template<unsigned int N> class TopTask : public MiddleTask<N-1> {
    public:
    TopTask(TaskCables< TypedWires<std::atomic<float>>,TypedWires<std::atomic<int>>,TypedWires<std::atomic<double>> >& All) : MiddleTask<N-1>(All)
    /*, _taskCable(std::get<
    0
    >(All))*/
    {}
    private:
    //TypedWires<bool>& _taskCable;
};

int main (){
    auto Dummy = TypedWires<>{};
    auto Dummy2 = TypedWires<>{};
    auto NoDummy = TaskCables<>{};
    auto OneDummy = TaskCables<TypedWires<>>{Dummy};
    auto TwoDummies = TaskCables<TypedWires<>,TypedWires<>>{Dummy,Dummy2};

    //auto NoTask = Task<>(NoDummy);
    
    auto FirstTask = Task<TypedWires<>>(OneDummy);

    auto FirstOfAll = Task<TypedWires<>,TypedWires<>>(TwoDummies);
    auto SecondOfAll = Task<TypedWires<>>(FirstOfAll);

    TypedWires<float> TestWire1 = TypedWires<std::atomic<float>>{1.2};
    TypedWires<int> TestWire2 = TypedWires<std::atomic<int>>{5};
    TypedWires<double> TestWire3 = TypedWires<std::atomic<double>>{4.7};
    auto AllTheDummies = TaskCables< TypedWires<std::atomic<float>>,TypedWires<std::atomic<int>>,TypedWires<std::atomic<double>> >
    {
        TestWire1,
        TestWire2,
        TestWire3
    };
    auto Task1 = TopTask<2>(AllTheDummies);
}