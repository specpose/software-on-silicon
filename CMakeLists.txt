cmake_minimum_required (VERSION 3.20)

set(CMAKE_CXX_STANDARD 14)

project(software-on-silicon)
#find_library(stackable-functor-allocation_FOUND stackable-functor-allocation REQUIRED)
find_package(ALSA)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads QUIET)

if (WIN32)
    if (NOT CMAKE_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif ()
endif ()

add_executable(test_EventLoop)
target_compile_definitions(test_EventLoop PRIVATE MEASUREMENT_UNIT_IN_MILLIS=100)
target_include_directories(test_EventLoop
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_EventLoop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_eventloop.cpp)
target_link_libraries(test_EventLoop PRIVATE Threads::Threads)
add_executable(test_Controller)
target_compile_definitions(test_Controller PRIVATE MEASUREMENT_UNIT_IN_MILLIS=100)
target_include_directories(test_Controller
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_Controller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_controller.cpp)
target_link_libraries(test_Controller PRIVATE Threads::Threads)
add_executable(test_RingBuffer)
target_compile_definitions(test_RingBuffer PRIVATE SAMPLE_TYPE=float)
target_compile_definitions(test_RingBuffer PRIVATE MAX_BLINK=1)
target_include_directories(test_RingBuffer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_RingBuffer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ringbuffer.cpp)
target_link_libraries(test_RingBuffer PRIVATE Threads::Threads)
add_executable(test_RWInterleaved)
target_include_directories(test_RWInterleaved
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_RWInterleaved PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_rwinterleaved.cpp)
target_link_libraries(test_RWInterleaved PRIVATE Threads::Threads ALSA::ALSA)
add_executable(test_MemoryController)
target_compile_definitions(test_MemoryController PRIVATE NUM_CHANNELS=5)
target_compile_definitions(test_MemoryController PRIVATE SAMPLE_TYPE=float)
target_compile_definitions(test_MemoryController PRIVATE BLOCK_SIZE=1000)
target_compile_definitions(test_MemoryController PRIVATE TOTAL_TIME=10)
target_compile_definitions(test_MemoryController PRIVATE STORAGE_SIZE=\(TOTAL_TIME*BLOCK_SIZE\))
target_include_directories(test_MemoryController
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_MemoryController PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_memorycontroller.cpp)
target_link_libraries(test_MemoryController PRIVATE Threads::Threads)
add_executable(test_RingToMemory)
target_compile_definitions(test_RingToMemory PRIVATE NUM_CHANNELS=2)
target_compile_definitions(test_RingToMemory PRIVATE SAMPLE_TYPE=short)
target_compile_definitions(test_RingToMemory PRIVATE MAX_BLINK=9600)
if (ALSA_DRIVER STREQUAL "snd_hda_intel")
    target_compile_definitions(test_RingToMemory PRIVATE SAMPLE_RATE=48000)
else()
    target_compile_definitions(test_RingToMemory PRIVATE SAMPLE_RATE=8000)
endif()
target_compile_definitions(test_RingToMemory PRIVATE BLOCK_SIZE=SAMPLE_RATE)
target_compile_definitions(test_RingToMemory PRIVATE TOTAL_TIME=160)
target_compile_definitions(test_RingToMemory PRIVATE STORAGE_SIZE=\(5*BLOCK_SIZE\))
target_include_directories(test_RingToMemory
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_RingToMemory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ringtomemory.cpp)
target_link_libraries(test_RingToMemory PRIVATE Threads::Threads)
add_executable(test_Poll)
target_include_directories(test_Poll
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_Poll PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_poll.cpp)
target_link_libraries(test_Poll PRIVATE Threads::Threads ALSA::ALSA)
add_executable(test_MCUFPGA)
target_include_directories(test_MCUFPGA
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_MCUFPGA PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_mcufpga.cpp)
target_link_libraries(test_MCUFPGA PRIVATE Threads::Threads)

#add_library(${PROJECT_NAME} STATIC)
#target_include_directories(${PROJECT_NAME}
#    PUBLIC
#        impl
#        include
#)
#target_sources(${PROJECT_NAME}
#    PUBLIC
#        test/test_ringtomemory.cpp
#)
#target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
