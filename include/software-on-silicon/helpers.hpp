namespace SOS {
    namespace MemoryView {
            class SignalsImpl : public SOS::MemoryView::Signals<1> {
                public:
                enum {
                    blink
                };
        };
        template<size_t Number,typename Signal>static bool get(Signal& mySignals) {
            auto stateQuery = std::get<Number>(mySignals).test_and_set();
            if (!stateQuery)
                std::get<Number>(mySignals).clear();
            return stateQuery;
        }
    }
}

