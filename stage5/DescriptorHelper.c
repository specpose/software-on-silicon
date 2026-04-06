//gcc -std=c99 DescriptorHelper.cpp
#define LOWER_STATES 1
#define UPPER_STATES 5
#define NUM_IDS 64-UPPER_STATES-LOWER_STATES

struct DMADescriptor {
    unsigned char id;// = { NUM_IDS };
    void* obj;// = nullptr;
    unsigned long obj_size;// = { 0 };
};

struct DescriptorHelper {
    // no user-provided, inherited, or explicit constructors
    // no private or protected direct non-static data members
    unsigned char size() { return count; }
    const DMADescriptor& operator[](const unsigned char i) const { return arr[i]; };
    DMADescriptor& operator[](const unsigned char i) { return arr[i]; };
    struct DMADescriptor arr[NUM_IDS];
    unsigned char count;// = 0;
};

#include "stdio.h"
int main() {
    int a;
    short b;
    int c[5];
    struct DescriptorHelper myHelper = { {{0, &a, sizeof(a)}, {0, &b, sizeof(b)}, {0, &c, sizeof(c)}}, 3 };
    for (unsigned char i = 0; i < myHelper.count; i++) {
        printf("DMAObject %d ptr: %d size: %d\n",i,myHelper.arr[i].obj,myHelper.arr[i].obj_size);
    }
}