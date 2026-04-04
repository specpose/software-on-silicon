cmake_minimum_required (VERSION 3.20)

project(alsa-code LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)

find_package(ALSA)

if (NOT DEFINED ALSA_DRIVER)
  set(ALSA_DRIVER "snd_hda_intel")
endif()
message("ALSA_DRIVER is ${ALSA_DRIVER}")
add_compile_definitions(PRIVATE NUM_CHANNELS=2)
if (ALSA_DRIVER STREQUAL "snd_hda_intel")
  add_compile_definitions(PRIVATE INTEL=1)
  add_compile_definitions(PRIVATE SAMPLE_RATE=48000)
else()
  add_compile_definitions(PRIVATE SAMPLE_RATE=8000)
endif()

add_executable(RW_INTERLEAVED)
target_compile_definitions(RW_INTERLEAVED PRIVATE SAMPLE_TYPE=short)
target_compile_definitions(RW_INTERLEAVED PRIVATE TOTAL_TIME=10)
target_compile_definitions(RW_INTERLEAVED PRIVATE STORAGE_SIZE=\(TOTAL_TIME*SAMPLE_RATE\))
if (ALSA_DRIVER STREQUAL "snd_hda_intel")
    target_compile_definitions(RW_INTERLEAVED PRIVATE MAX_READ=2)
    target_compile_definitions(RW_INTERLEAVED PRIVATE MAX_BLINK=2)
else()
    target_compile_definitions(RW_INTERLEAVED PRIVATE MAX_READ=4)
    target_compile_definitions(RW_INTERLEAVED PRIVATE MAX_BLINK=4)
endif()
target_include_directories(RW_INTERLEAVED
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_sources(RW_INTERLEAVED PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/RW_INTERLEAVED.cpp)
target_link_libraries(RW_INTERLEAVED PRIVATE ALSA::ALSA)

add_executable(MMAP_INTERLEAVED)
target_compile_definitions(MMAP_INTERLEAVED PRIVATE SAMPLE_TYPE=short)
target_compile_definitions(MMAP_INTERLEAVED PRIVATE TOTAL_TIME=10)
target_compile_definitions(MMAP_INTERLEAVED PRIVATE STORAGE_SIZE=\(TOTAL_TIME*SAMPLE_RATE\))
if (ALSA_DRIVER STREQUAL "snd_hda_intel")
    target_compile_definitions(MMAP_INTERLEAVED PRIVATE MAX_READ=32)
    target_compile_definitions(MMAP_INTERLEAVED PRIVATE MAX_BLINK=32)
else()
    target_compile_definitions(MMAP_INTERLEAVED PRIVATE MAX_READ=8)
    target_compile_definitions(MMAP_INTERLEAVED PRIVATE MAX_BLINK=8)
endif()
target_include_directories(MMAP_INTERLEAVED
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_sources(MMAP_INTERLEAVED PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/MMAP_INTERLEAVED.cpp)
target_link_libraries(MMAP_INTERLEAVED PRIVATE ALSA::ALSA)
add_executable(MMAP_NONINTERLEAVED)
target_compile_definitions(MMAP_NONINTERLEAVED PRIVATE SAMPLE_TYPE=short)
target_compile_definitions(MMAP_NONINTERLEAVED PRIVATE TOTAL_TIME=10)
target_compile_definitions(MMAP_NONINTERLEAVED PRIVATE STORAGE_SIZE=\(TOTAL_TIME*SAMPLE_RATE\))
target_compile_definitions(MMAP_NONINTERLEAVED PRIVATE MAX_READ=8)
target_compile_definitions(MMAP_NONINTERLEAVED PRIVATE MAX_BLINK=8)
target_include_directories(MMAP_NONINTERLEAVED
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_sources(MMAP_NONINTERLEAVED PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/MMAP_NONINTERLEAVED.cpp)
target_link_libraries(MMAP_NONINTERLEAVED PRIVATE ALSA::ALSA)

add_executable(POLL)
target_compile_definitions(POLL PRIVATE SAMPLE_TYPE=short)
target_compile_definitions(POLL PRIVATE TOTAL_TIME=10)
target_compile_definitions(POLL PRIVATE BLOCK_SIZE=SAMPLE_RATE)
if (ALSA_DRIVER STREQUAL "snd_hda_intel")
    target_compile_definitions(POLL PRIVATE MAX_READ=32)
    target_compile_definitions(POLL PRIVATE MAX_BLINK=9600)
else()
    target_compile_definitions(POLL PRIVATE MAX_READ=8)
    target_compile_definitions(POLL PRIVATE MAX_BLINK=9600)
endif()
target_include_directories(POLL
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_sources(POLL PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/POLL.cpp)
target_link_libraries(POLL PRIVATE ALSA::ALSA)