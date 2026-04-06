namespace SOS {
    namespace Protocol {
        struct DescriptorHelper {
            // no user-provided, inherited, or explicit constructors
            // no private or protected direct non-static data members
            //            std::size_t size();
            std::size_t size() { return 3; }
            //std::size_t size() { return count; }
/*           void set_count( std::size_t num ) {
             *               if ( num <= NUM_IDS )
             *                   count = num;
             *               //else
             *               //    SFA::util::runtime_error(SFA::util::error_code::DescriptorCountOutOfBounds, __FILE__, __func__, typeid(*this).name());
            }
*/
            const DMADescriptor& operator[](const std::size_t i) const { return arr[i]; };
            DMADescriptor& operator[](const std::size_t i) { return arr[i]; };
            //            std::size_t count = 0;
            std::array<DMADescriptor, NUM_IDS> arr;
        };
        //std::size_t DescriptorHelper::size() { return 3; );
    }
}