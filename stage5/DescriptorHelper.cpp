//g++ -std=c++11 DescriptorHelper.cpp
static const unsigned char LOWER_STATES = 1;
static const unsigned char UPPER_STATES = 5;
static const unsigned char NUM_IDS = 64 - UPPER_STATES - LOWER_STATES;

#include <array>
struct DMADescriptor {
    DMADescriptor() { } // DANGER
    DMADescriptor(unsigned char id, void* obj, std::size_t obj_size)
    : id(id)
    , obj(obj)
    , obj_size(obj_size)
    {}
    unsigned char id = NUM_IDS;
    void* obj = nullptr;
    std::size_t obj_size = 0;
};

#include <initializer_list>
namespace SOS {
    namespace Protocol {
        struct DescriptorHelper {
            // no user-provided, inherited, or explicit constructors
            // no private or protected direct non-static data members
//            std::size_t size();
            std::size_t size() { return 3; }
//            std::size_t size() { return count; }
/*            void set_count( std::size_t num ) {
                if ( num <= NUM_IDS )
                    count = num;
                //else
                //    SFA::util::runtime_error(SFA::util::error_code::DescriptorCountOutOfBounds, __FILE__, __func__, typeid(*this).name());
            }*/
            const DMADescriptor& operator[](const std::size_t i) const { return arr[i]; };
            DMADescriptor& operator[](const std::size_t i) { return arr[i]; };
//            std::size_t count = 0;
            std::array<DMADescriptor, NUM_IDS> arr;
        };
//        std::size_t DescriptorHelper::size() { return 3; );
    }
}

#include <string>
#include <iostream>
int main() {
    int a;
    std::string b;
    std::array<int,5> c;
    //std::array<DMADescriptor, NUM_IDS> myHelper = { DMADescriptor(0, &a, sizeof(a)), DMADescriptor(0, &b, sizeof(b)), DMADescriptor(0, &c, sizeof(c)) };
    SOS::Protocol::DescriptorHelper myHelper = { DMADescriptor(0, &a, sizeof(a)), DMADescriptor(0, &b, sizeof(b)), DMADescriptor(0, &c, sizeof(c)) };
//    myHelper.set_count(3);
    for (std::size_t i = 0; i < myHelper.size(); i++) {
        std::cout << "DMAObject " << i << " ptr: " << (myHelper)[i].obj << " size: " << (myHelper)[i].obj_size << std::endl;
    }
}