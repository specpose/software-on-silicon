cmake_minimum_required (VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

project(software-on-silicon)
#find_library(stackable-functor-allocation_FOUND stackable-functor-allocation REQUIRED)

add_executable(test_EventLoop)
target_include_directories(test_EventLoop
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
target_sources(test_EventLoop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/EventLoop.cpp)
add_executable(test_Controller)
target_include_directories(test_Controller
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
target_sources(test_Controller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/Controller.cpp)
add_executable(test_RingToMemory)
target_include_directories(test_RingToMemory
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
target_sources(test_RingToMemory PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/RingToMemory.cpp)
add_executable(test_RingBuffer)
target_include_directories(test_RingBuffer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
target_sources(test_RingBuffer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/RingBuffer.cpp)
add_executable(test_HostMemoryController)
target_include_directories(test_HostMemoryController
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
)
target_sources(test_HostMemoryController PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/HostMemoryController.cpp)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME}
    PUBLIC
      FILE_SET public_headers
      TYPE HEADERS
      FILES
        $<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
)
install(TARGETS ${PROJECT_NAME} FILE_SET public_headers)
