#include <tuple>
#include <atomic>
#include <thread>
#include <functional>

namespace SOS{
    namespace MemoryView {
        template<typename T> class TypedWires : public std::tuple<std::atomic<T>> {
            
        };
    }
    template<typename T> class Thread : private std::jthread {
        public:
        Thread(T functor) : functorCopy(functor) {
            auto thread = std::jthread{std::mem_fn(&T::operator()),functorCopy};
            this->swap(thread);
        };
        ~Thread() {this->join();}
        virtual void operator()() final{
            if (this->joinable())
                this->join();
        };
        private:
        T functorCopy;
    };
    namespace Behavior {
        template<typename T> class EventHandler {
            public:
            EventHandler(SOS::MemoryView::TypedWires<T>& databus) : _wires(databus) {}
            virtual void operator()()=0;
            protected:
            SOS::MemoryView::TypedWires<T>& _wires;
        };
    }
}