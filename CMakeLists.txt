cmake_minimum_required (VERSION 3.23)

set(CMAKE_CXX_STANDARD 20)

project(software-on-silicon)

add_executable(test_ProcessorArray)
target_include_directories(test_ProcessorArray PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_sources(test_ProcessorArray PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/ProcessorArray.cpp)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME}
    PUBLIC
      FILE_SET public_headers
      TYPE HEADERS
      FILES
        $<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
)
install(TARGETS ${PROJECT_NAME} FILE_SET public_headers)
