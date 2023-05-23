cmake_minimum_required (VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)

project(software-on-silicon)
add_library(${PROJECT_NAME} INTERFACE)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
)
target_sources(${PROJECT_NAME}
    PUBLIC
      FILE_SET public_headers
      TYPE HEADERS
      FILES
        ProcessorArray.h
)
install(TARGETS ${PROJECT_NAME} FILE_SET public_headers)