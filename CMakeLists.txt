cmake_minimum_required (VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(stackable-functor-allocation)

project(software-on-silicon)
#find_library(stackable-functor-allocation_FOUND stackable-functor-allocation REQUIRED)

add_executable(test_ProcessorArray)
target_include_directories(test_ProcessorArray PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_sources(test_ProcessorArray PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/ProcessorArray.cpp)
add_executable(test_TypedWires)
target_include_directories(test_TypedWires
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/stackable-functor-allocation/include/
)
target_sources(test_TypedWires PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/TypedWires.cpp)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME}
    PUBLIC
      FILE_SET public_headers
      TYPE HEADERS
      FILES
        $<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
)
install(TARGETS ${PROJECT_NAME} FILE_SET public_headers)
