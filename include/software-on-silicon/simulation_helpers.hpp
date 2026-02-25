#include <chrono>

using namespace std::chrono;

template <typename DurationType,
    unsigned int Period>
class Timer : public SOS::Behavior::EventDummy<> {
public:
    Timer(SOS::MemoryView::BusShaker::signal_type& bussignal)
        : SOS::Behavior::EventDummy<>(bussignal)
    {
        _thread = start(this);
    }
    ~Timer()
    {
        destroy(_thread);
        std::cout << "Timer spent " << duration_cast<DurationType>(t_counter - duration_cast<high_resolution_clock::duration>(DurationType { (runCount * Period) })).count() << " duration units more than expected." << std::endl;
        std::cout << "Timer has been run " << runCount << " times." << std::endl;
        std::cout << "Timer spent " << duration_cast<nanoseconds>(t_counter - runCount * duration_cast<high_resolution_clock::duration>(DurationType { Period })).count()
                  << "ns more on average per " << Period << " duration units." << std::endl;
    }
    void event_loop()
    {
        if (!_intrinsic.getUpdatedRef().test_and_set()) {
            const auto t_start = high_resolution_clock::now();
            const auto c_start = clock();
            operator()();
            c_counter += clock() - c_start;
            t_counter += high_resolution_clock::now() - t_start;
            runCount++;
            _intrinsic.getAcknowledgeRef().clear();
        }
    }

protected:
    virtual void operator()() = 0;

private:
    int runCount = 0;
    clock_t c_counter = 0;
    high_resolution_clock::duration t_counter = high_resolution_clock::duration {};

    std::thread _thread = std::thread {};
};

template <typename DurationType, unsigned int Period>
class TimerIF : public virtual Timer<DurationType, Period> {
public:
    using Timer<DurationType, Period>::Timer;
};

template <typename DurationType, unsigned int Period>
class SystemTimer : public TimerIF<DurationType, Period> {
public:
    SystemTimer(SOS::MemoryView::BusShaker::signal_type& bussignal)
        : TimerIF<DurationType, Period>(bussignal)
        , Timer<DurationType, Period>(bussignal)
    {
    }

protected:
    virtual void operator()() final
    {
        std::this_thread::sleep_for(typename DurationType::duration { Period });
        ;
    }
};

template <typename DurationType, unsigned int Period>
class TickTimer : public TimerIF<DurationType, Period> {
public:
    TickTimer(SOS::MemoryView::BusShaker::signal_type& bussignal, unsigned long ticks)
        : _ticks(Period * ticks)
        , TimerIF<DurationType, Period>(bussignal)
        , Timer<DurationType, Period>(bussignal)
    {
    }
    virtual void operator()() final
    {
        for (unsigned long i = 0; i < _ticks; ++i) { }
    }

private:
    const unsigned long _ticks;
};
