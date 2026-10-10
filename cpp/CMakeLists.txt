cmake_minimum_required(VERSION 3.16)

project(debug_overlay_hud
    VERSION 0.2.0
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

add_library(debug_overlay_core STATIC
    src/HudWindow.cpp
    src/HudWindow.h
    src/DisplaySnapshot.h
    src/ProcessIdentity.h
    src/LinuxProcess.cpp
    src/LinuxProcess.h
    src/FocusedWindowProvider.h
    src/GnomePidFileProvider.cpp
    src/GnomePidFileProvider.h
    src/FocusSampler.cpp
    src/FocusSampler.h)

target_include_directories(debug_overlay_core PUBLIC src)
target_link_libraries(debug_overlay_core PUBLIC Qt6::Widgets)
target_compile_options(debug_overlay_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)

add_executable(debug_overlay_hud src/main.cpp)
target_link_libraries(debug_overlay_hud PRIVATE debug_overlay_core)

enable_testing()

add_executable(debug_overlay_tests tests/test_hud_buttons.cpp)
target_link_libraries(debug_overlay_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_focus_tests tests/test_focus.cpp)
target_link_libraries(debug_overlay_focus_tests PRIVATE debug_overlay_core)

add_test(NAME hud_buttons COMMAND debug_overlay_tests)
add_test(NAME focus COMMAND debug_overlay_focus_tests)

set_tests_properties(hud_buttons focus PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
