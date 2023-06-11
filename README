- [ ] Test structure for manual conversion to fpga descriptor language
- [ ] Test compile to fpga descriptor language
- [ ] Finalize header interface
- [x] Test gcc
- [ ] Test MSVC
- [ ] Test clang

# Software On Silicon
Is a refactoring pattern. It is trying to establish building blocks that can some day be run on an fpga. Eventually there could be a standard interface for integrating system on chip (SOC).
## Algorithm LifeCycle
1. C++20/STL
   - If you have implemented STL algorithms and are looking for ways to accelerate code, there is a span now, that can be used to partition vectors for GPU kernels. Whether it is a vector or an array is not entirely clear. Effectively it is re-introducing a storage-like class into algorithms which have been originally designed to abstract away the storage class. Keep in mind that a GPU is always a rigid hardware implementation and the scope of the API is not a response to future challenges.
   - The sized_sentinel_for concept of the ranges API presents another approach to address the same problem. The ranges call syntax is similar to Java and is a radical change in programming paradigm.
2. C++17/SFA
   - SFA is a refactoring pattern which also has a Java-like call syntax. SOS specifically deals with signaling which theoretically could be compiled to Verilog wires. Applying stackable functor allocation may lead to code which is compatible to ranges, but ranges has a few abstractions which make it difficult to write low-level code. Although SOS presents an approach to run code on fpga, it is not the ideal language to translate to an fpga descriptor language. However, it may be easier to integrate with build systems and provide low-level C functionality, for example for compiling against templated extern static objects (handles).
   - It is object-oriented. It helps converting STL programs into something that ressembles a C program, but keep in mind that object-oriented design *directly* reflects the need to bind registers and memory controllers *locally* to functions. The class body is an abstraction of memory visibility and inheritance is an abstraction of behavior.
3. Java5/SFA
   - This is the method of choice for new algorithms. Java provides a Runnable class that adheres more strictly to the design pattern of the C++ thread. It enforces the separation of the creation of threads and the setup of communication between the threads. It also has Factory design patterns, Generics and introspection. An explicit definition of C++ atomic should not be needed, primitive data-types should automatically be atomic in Java.

To install:
```bash
mkdir build
cd build
mkdir -p ~/usr/local/
cmake --install-prefix=`echo ~`/usr/local ../
cmake --build . --clean-first --verbose
cmake --install . --verbose
```
