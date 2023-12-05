#include "EventLoop.hpp"
#include <chrono>

using namespace std::chrono;

//Loops always have at least one signal for termination acknowledge if used for single run
template<typename T, typename S = typename std::enable_if<
        std::is_base_of< SOS::Behavior::subcontroller_tag,T >::value,T
        >::type > class _Thread {
    public:
    using subcontroller_type = S;
    _Thread() {}
};
template<typename S> class Thread : private _Thread<S> {
    public:
    //using subcontroller_type = S;
    Thread() : _Thread<S>(), _child(typename _Thread<S>::subcontroller_type{_foreign}) {}
    protected:
    typename _Thread<S>::subcontroller_type::bus_type _foreign = typename _Thread<S>::subcontroller_type::bus_type{};
    typename _Thread<S>::subcontroller_type _child;
};
/*template<> class Thread<SOS::Behavior::DummyController> {
    public:
    using subcontroller_type = SOS::Behavior::DummyController;
    Thread() {}
    virtual ~Thread() {};
};*/
template<typename S, typename PassthruBusType, typename... Others> class PassthruThread : private _Thread<S> {
    public:
    //using subcontroller_type = S;
    PassthruThread(typename _Thread<S>::subcontroller_type::bus_type& blocker, PassthruBusType& passThru, Others&... args) :
     _Thread<S>(), _foreign(passThru), _child(typename _Thread<S>::subcontroller_type{blocker, _foreign, args...}) {}
    protected:
    PassthruBusType& _foreign;
    typename _Thread<S>::subcontroller_type _child;
};

//error: non-type template parameters of class type only available with ‘-std=c++20’ or ‘-std=gnu++20’
//template<typename DurationType, typename DurationType::period> class Timer {
template<typename DurationType,
        unsigned int Period,
        typename PeriodType = typename std::enable_if<
            true, typename DurationType::duration
            >::type
        > class Timer : private SOS::Behavior::DummyController<SOS::MemoryView::BusShaker::signal_type>, public SOS::Behavior::Loop {//no bus here
    public:
    Timer(SOS::MemoryView::BusShaker::signal_type& bussignal) :
    SOS::Behavior::DummyController<SOS::MemoryView::BusShaker::signal_type>(bussignal),
    SOS::Behavior::Loop() {
        _thread = start(this);
    }
    ~Timer(){
        stop_token.getUpdatedRef().clear();
        _thread.join();
        std::cout<<"Timer spent "<<duration_cast<DurationType>(t_counter -
        duration_cast<high_resolution_clock::duration>(DurationType{(runCount * Period)})
        ).count()<<" duration units more than expected."<<std::endl;
        std::cout<<"Timer has been run "<<runCount<<" times."<<std::endl;
        std::cout<<"Timer spent "<<
        duration_cast<nanoseconds>(t_counter-
        runCount * duration_cast<high_resolution_clock::duration>(DurationType{Period})
        ).count()
        <<"ns more on average per "<<Period<<" duration units."<<std::endl;
    }
    void event_loop(){
        while(stop_token.getUpdatedRef().test_and_set()){
        if (!_intrinsic.getUpdatedRef().test_and_set()){
            const auto t_start = high_resolution_clock::now();
            const auto c_start = clock();
            operator()();
            c_counter += clock() - c_start;
            t_counter += high_resolution_clock::now() - t_start;
            runCount++;
            _intrinsic.getAcknowledgeRef().clear();
        }
        }
        stop_token.getAcknowledgeRef().clear();
    }
    void constexpr operator()(){
        std::this_thread::sleep_for(DurationType{Period});;
    }
    private:
    int runCount = 0;
    clock_t c_counter = 0;
    high_resolution_clock::duration t_counter = high_resolution_clock::duration{};

    std::thread _thread = std::thread{};
};
