cmake_minimum_required(VERSION 3.20)
project(untitled15)

set(CMAKE_CXX_STANDARD 20)

add_executable(untitled15
    src/main.cpp
    src/overlay.cpp

    # GUI — one file per responsibility
    src/gui_tab_config.cpp
    src/gui_tab_visuals.cpp
    src/gui_tab_memory.cpp
    src/gui_tab_hotkeys.cpp
    src/gui_tab_debug.cpp
    src/gui_tab_about.cpp
    src/gui_tab_players.cpp
    src/player_scanner.cpp
    src/bomb_found.cpp
    src/aim.cpp
)

target_include_directories(untitled15 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(untitled15 PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(untitled15 PRIVATE
    user32
    gdi32
    psapi
    comctl32
    dwmapi
)

set_target_properties(untitled15 PROPERTIES
    WIN32_EXECUTABLE TRUE
)
