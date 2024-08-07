cmake_minimum_required (VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

project(software-on-silicon)
#find_library(stackable-functor-allocation_FOUND stackable-functor-allocation REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads QUIET)

add_executable(test_EventLoop)
target_include_directories(test_EventLoop
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_EventLoop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_eventloop.cpp)
target_link_libraries(test_EventLoop PRIVATE Threads::Threads)
add_executable(test_Controller)
target_include_directories(test_Controller
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_Controller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_controller.cpp)
target_link_libraries(test_Controller PRIVATE Threads::Threads)
add_executable(test_RingBuffer)
target_include_directories(test_RingBuffer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_RingBuffer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ringbuffer.cpp)
target_link_libraries(test_RingBuffer PRIVATE Threads::Threads)
add_executable(test_MemoryController)
target_include_directories(test_MemoryController
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_MemoryController PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_memorycontroller.cpp)
target_link_libraries(test_MemoryController PRIVATE Threads::Threads)
add_executable(test_RingToMemory)
target_include_directories(test_RingToMemory
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_RingToMemory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ringtomemory.cpp)
target_link_libraries(test_RingToMemory PRIVATE Threads::Threads)
add_executable(test_MCUFPGA)
target_include_directories(test_MCUFPGA
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/
)
target_sources(test_MCUFPGA PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/test_mcufpga.cpp)
target_link_libraries(test_MCUFPGA PRIVATE Threads::Threads)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME}
    PUBLIC
      FILE_SET public_headers
      TYPE HEADERS
      FILES
        $<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
)
install(TARGETS ${PROJECT_NAME} FILE_SET public_headers)
