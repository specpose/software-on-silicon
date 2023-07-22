- [ ] Test structure for manual conversion to fpga descriptor language
- [ ] Implement compile to fpga descriptor language
- [ ] Finalize header interface
- [x] Test gcc
- [x] Test MSVC
- [ ] Test clang
# Software On Silicon
Is a digital twin for an electronic circuit. It is trying to establish building blocks that can some day be run on an fpga. Eventually there could be a standard interface for integrating system on chip (SOC).
## Algorithm LifeCycle
1. C++17/SOS
   - SOS specifically deals with *signaling* which theoretically could be compiled to Verilog wires.
   - Although SOS presents an approach to run code on fpga, it is not the ideal language to translate to an fpga descriptor language.
   - It may be easier to integrate with build systems than Java5/SOS and provide low-level C functionality, for example for compiling against templated extern static objects (handles).
2. Java5/SOS
   - This is the method of choice for new algorithms. Java provides a Runnable class that adheres more strictly to the design pattern of the C++ thread. It enforces the separation of the creation of threads and the setup of communication between the threads. The Generics allow for perfect forwarding of the SubController. An explicit definition of C++ atomic should not be needed, primitive data-types should automatically be atomic in Java.
   - Java's reflect machinery offers the (Class)this.getName() method. It is more convenient for throwing XML SignalDumps than the C typeid(this).name() method.
### Installation
```sh
mkdir build
cd build
mkdir -p ~/usr/local/
cmake --install-prefix=`echo ~`/usr/local ../
cmake --build . --clean-first --verbose
cmake --install . --verbose
```
