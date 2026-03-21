cmake_minimum_required(VERSION 3.20)
project(untitled15)

set(CMAKE_CXX_STANDARD 20)

add_executable(untitled15
    main.cpp
    overlay.cpp

    # GUI — one file per responsibility
    gui.cpp
    gui_draw.cpp
    gui_tab_config.cpp
    gui_tab_visuals.cpp
    gui_tab_memory.cpp
    gui_tab_hotkeys.cpp
    gui_tab_debug.cpp
    gui_tab_about.cpp
    gui_tab_players.cpp
    player_scanner.cpp
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
