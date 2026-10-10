cmake_minimum_required(VERSION 3.16)

project(debug_overlay_hud
    VERSION 0.1.0
    DESCRIPTION "Debug Overlay HUD (Qt 6 Widgets)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(debug_overlay_hud
    src/main.cpp
    src/HudWindow.cpp
    src/HudWindow.h)

target_include_directories(debug_overlay_hud PRIVATE src)

target_link_libraries(debug_overlay_hud PRIVATE Qt6::Widgets)

target_compile_options(debug_overlay_hud PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)
