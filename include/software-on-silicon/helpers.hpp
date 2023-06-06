namespace SOS {
    namespace MemoryView {
        template<size_t Number,typename Signal>static bool get(Signal& mySignals) {
            auto stateQuery = std::get<Number>(mySignals).test_and_set();
            if (!stateQuery)
                std::get<Number>(mySignals).clear();
            return stateQuery;
        }
    }
}

