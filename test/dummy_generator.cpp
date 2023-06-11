#include <iostream>
#include <tuple>
#include <atomic>

template<typename T> struct Bus {
    using signal_type = std::array<std::atomic_flag,1>;
    using wire_type = std::tuple< std::tuple<std::atomic<int>,std::atomic<int>>, std::tuple<std::atomic<float>> >;
    signal_type signal;
    wire_type wire;
};

class EventLoop {
    public:
    EventLoop(Bus<EventLoop>::signal_type& ground) : intrinsic(ground){}
    private:
    typename Bus<EventLoop>::signal_type& intrinsic;
};

class DummyController {
    public:
    DummyController(Bus<DummyController>& myBus) {}
};

template<
typename SubController
>class Controller : EventLoop {
    public:
    Controller(Bus<Controller>& mySelf) : EventLoop(mySelf.signal), intrinsic(std::get<0>(mySelf.wire)), foreign(Bus<SubController>{}), child(SubController(foreign)) {};
    private:
    //typename: can not derive subtype of tuple0
    typename Bus<Controller>::task_type& intrinsic;
    Bus<SubController> foreign;
    SubController child;
};

template<> struct Bus<Controller<DummyController>> : public Bus<EventLoop>{
        using task_type = decltype(std::get<0>(wire));
};

//typename: name of struct can not be automatically derived
struct TaskBus : public Bus<Controller<DummyController>> {
        using task_type = decltype(std::get<1>(wire));
};

template<
typename SubController,unsigned int INeedNumberOfProcessors
>class Task : public Controller<SubController> {
    public:
    Task(TaskBus& meAndMyBase) : Controller<SubController>(meAndMyBase), intrinsic(std::get<1>(meAndMyBase.wire)) {}
    private:
    //typename: can not derive subtype of tuple1
    typename TaskBus::task_type& intrinsic;
};

int main(){
    auto myBus = TaskBus{};
    auto myController = Task<DummyController,1>(myBus);
}