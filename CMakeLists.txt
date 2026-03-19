cmake_minimum_required(VERSION 3.20)
project(untitled15)

set(CMAKE_CXX_STANDARD 20)

add_executable(untitled15
        main.cpp
        overlay.cpp
)

target_include_directories(untitled15 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# NOMINMAX must be set globally so windows.h never defines min/max macros,
# regardless of which .cpp includes it first
target_compile_definitions(untitled15 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_link_libraries(untitled15 PRIVATE
        user32
        gdi32
        psapi
)

set_target_properties(untitled15 PROPERTIES
        WIN32_EXECUTABLE TRUE
)