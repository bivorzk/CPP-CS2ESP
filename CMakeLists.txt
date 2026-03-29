cmake_minimum_required(VERSION 3.20)
project(untitled15)

set(CMAKE_CXX_STANDARD 20)

add_executable(untitled15
    src/main.cpp
    src/app/app.cpp
    src/aim/aim_aux.cpp
    src/overlay/overlay.cpp
    src/aim/aim.cpp

    # GUI — core + tabs
    src/gui/gui.cpp
    src/gui/gui_draw.cpp
    src/gui/gui_tab_config.cpp
    src/gui/gui_tab_visuals.cpp
    src/gui/gui_tab_memory.cpp
    src/gui/gui_tab_hotkeys.cpp
    src/gui/gui_tab_debug.cpp
    src/gui/gui_tab_about.cpp
    src/gui/gui_tab_players.cpp
    src/game/player_scan/player_scanner.cpp
    src/game/player_scan/player_scanner_helpers.cpp
    src/game/bomb/bomb_found.cpp
    src/game/bhop/bhop.cpp
)


target_include_directories(untitled15 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/aim
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/player_scan
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/bomb
)

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
