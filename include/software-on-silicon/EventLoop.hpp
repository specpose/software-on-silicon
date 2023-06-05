#include "stackable-functor-allocation/Thread.hpp"

#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        template<size_t N> class Signals : public std::array<std::atomic_flag,N> {};
        template<typename T, size_t N> class TypedWire : public std::array<std::atomic<T>,N>{};
    }
    namespace Behavior {
        template<typename T> class EventLoop{
            public:
            using SignalType = T;
            EventLoop(SignalType& signalbus) : _intrinsic(signalbus) {}
            virtual ~EventLoop(){}
            virtual void event_loop()=0;
            protected:
            template<typename C> std::thread start(C* startme){
                return std::move(std::thread{std::mem_fn(&C::event_loop),startme});
            }
            SignalType& _intrinsic;
        };
    }
}