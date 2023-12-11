- [ ] Test structure for manual conversion to fpga descriptor language
- [ ] Implement compile to fpga descriptor language
- [ ] Finalize header interface
- [x] Test gcc
- [x] Test MSVC
- [ ] Test clang
# Software On Silicon
Is a digital twin for an electronic circuit. It is trying to establish building blocks that can some day be run on an fpga. Eventually there could be a standard interface for integrating system on chip (SOC).
## Software LifeCycle
1. API
   - Pure algorithms can be implemented on a host platform. It can integrate with APIs in a limited scope. This is because the design pattern makes heavy use of threads and needs modifications for allocating memory and graceful shutdown.
2. CPU/CPU
   - It can be run in a hybrid mode using a serial interface between two traditional multitasking operating system.
3. CPU/MCU
   - It can be run on a real-time operating system if the compiler and threads are supported, eventually even in ROM/stack only.
4. MCU/FPGA
   - There should be a way to transpile code to an FPGA descriptor language in the future.
## [EventLoop](impl/EventLoop.cpp)
This example shows the use of listening for and clearing a notification signal. The signal is passed to a controller through a bundled constructor object. It also shows where the thread has to be instantiated.
## [Controller](impl/Controller.cpp)
This example shows the principal design pattern. It is based on *perfect forwarding* of types:
```
Controller1<SubController2<SubController3>>
```
This mostly follows the signaling pathway. Threads are not forced to be instantiated in this manner and other signaling pathways may be implemented. The memory is always kept at the base of the *template hierarchy* and passed to the subcontroller by reference.
## [MemoryController](impl/MemoryController.cpp)
This is an implementation of a FIFO read with a blocking write. The separation of tasks comes from the intent to not only localise memory and separate the signal pathway, but also to try to keep arithmetic units within the same processing unit (thread).
## [RingBuffer](impl/RingBuffer.cpp)
This is an implementation of a cached write with a random-access read. Templating is not only applied to the signaling hierarchy, but also to structured datatypes. The *class hierarchy* makes it easier to separate interfaces from implementation.
## [RingBuffer to MemoryController](impl/RingToMemory.cpp)
This is a combination of the ringbuffer and the memorycontroller. It has also been tested with memory allocations, but needs extensive modifications. Do a diff to the ARAFallback branch to get an idea of how many modifications are necessary.
## [MCUController to FPGAController](impl/MCUFPGA.cpp)
This is an implementation of a serial protocol using only 6 datawires and 2 internally used handshake-like wires to attach an FPGA. It maintains compatibility with a standard 8 datawire serial interface. The two internally used wires are used to rapidly transfer synchronisation states of DMA memory objects which are mirrored on both the MCU and FPGA top-(templating)-level controllers.
### Installation
```sh
mkdir build
cd build
mkdir -p ~/usr/local/
cmake --install-prefix=`echo ~`/usr/local ../
cmake --build . --clean-first --verbose
cmake --install . --verbose
```
