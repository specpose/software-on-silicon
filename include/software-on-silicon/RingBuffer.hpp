namespace SOS {
    namespace MemoryView {
        template<typename ArithmeticType> struct WriteBufferSize : public SOS::MemoryView::ConstCable<ArithmeticType,2> {
            WriteBufferSize(const ArithmeticType First, const ArithmeticType Second) : SOS::MemoryView::ConstCable<ArithmeticType,2>{First,Second} {}
            auto& getWriterStartRef(){return std::get<0>(*this);}
            auto& getWriterEndRef(){return std::get<1>(*this);}
        };
        template<typename ArithmeticType> struct RingBufferTaskCable : public SOS::MemoryView::TaskCable<ArithmeticType,2> {
            using SOS::MemoryView::TaskCable<ArithmeticType, 2>::TaskCable;
            auto& getCurrentRef(){return std::get<0>(*this);}
            auto& getThreadCurrentRef(){return std::get<1>(*this);}
        };
        template<typename OutputBuffer> struct RingBufferBus : public SOS::MemoryView::BusNotifier {
            using _pointer_type = typename OutputBuffer::iterator;
            using _difference_type = typename OutputBuffer::difference_type;
            using cables_type = std::tuple< SOS::MemoryView::RingBufferTaskCable<_pointer_type> >;
            using const_cables_type = std::tuple< WriteBufferSize<_pointer_type> >;
            RingBufferBus(const _pointer_type begin, const _pointer_type afterlast) :
            //tuple requires copy constructor for any tuple that isn't default constructed
            const_cables(
            std::tuple< WriteBufferSize<_pointer_type> >{WriteBufferSize<_pointer_type>(begin,afterlast)}
            )
            {
                if(std::distance(begin,afterlast)<2)
                    throw SFA::util::logic_error("Requested RingBuffer size not big enough.",__FILE__,__func__);
                //=>explicitly initialize wires
                std::get<0>(cables).getThreadCurrentRef().store(begin);
                auto next = begin;
                std::get<0>(cables).getCurrentRef().store(++next);
            }
            cables_type cables;
            const_cables_type const_cables;
        };
    }
    namespace Behavior {
        template <typename RingBufferType>class RingBufferTask {
            public:
            using cable_type = typename std::tuple_element<0,typename SOS::MemoryView::RingBufferBus<RingBufferType>::cables_type>::type;
            using const_cable_type = typename std::tuple_element<0,typename SOS::MemoryView::RingBufferBus<RingBufferType>::const_cables_type>::type;
            RingBufferTask(cable_type& indices, const_cable_type& bounds) :
            _item(indices),
            _bounds(bounds)
            {}
            void read_loop() {
                auto threadcurrent = _item.getThreadCurrentRef().load();
                auto current = _item.getCurrentRef().load();
                bool stop = false;
                while(!stop){//if: possible less writes than reads
                    ++threadcurrent;
                    if (threadcurrent==_bounds.getWriterEndRef())
                        threadcurrent=_bounds.getWriterStartRef();
                    if (threadcurrent!=current) {
                        write(*threadcurrent);
                        _item.getThreadCurrentRef().store(threadcurrent);
                    } else {
                        stop = true;
                    }
                }
            }
            protected:
            virtual void write(const char character)=0;
            private:
            cable_type& _item;
            const_cable_type& _bounds;
        };
    }
}