cmake_minimum_required (VERSION 3.20)

project(alsa-code LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)

find_package(ALSA)

add_executable(RW_INTERLEAVED)
target_include_directories(RW_INTERLEAVED
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_sources(RW_INTERLEAVED PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/RW_INTERLEAVED.cpp)
target_link_libraries(RW_INTERLEAVED PRIVATE ALSA::ALSA)

add_executable(MMAP_INTERLEAVED)
target_include_directories(MMAP_INTERLEAVED
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_sources(MMAP_INTERLEAVED PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/MMAP_INTERLEAVED.cpp)
target_link_libraries(MMAP_INTERLEAVED PRIVATE ALSA::ALSA)

add_executable(MMAP_NONINTERLEAVED)
target_include_directories(MMAP_NONINTERLEAVED
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_sources(MMAP_NONINTERLEAVED PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/MMAP_NONINTERLEAVED.cpp)
target_link_libraries(MMAP_NONINTERLEAVED PRIVATE ALSA::ALSA)

add_executable(POLL)
target_include_directories(POLL
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_sources(POLL PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/POLL.cpp)
target_link_libraries(POLL PRIVATE ALSA::ALSA)