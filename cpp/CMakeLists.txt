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

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)

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
    src/FocusSampler.h
    src/CpuSampler.cpp
    src/CpuSampler.h
    src/MemoryFormat.cpp
    src/MemoryFormat.h
    src/ProcessGrouper.cpp
    src/ProcessGrouper.h
    src/GroupCache.cpp
    src/GroupCache.h
    src/SamplerWorker.cpp
    src/SamplerWorker.h
    src/HealthEvaluator.cpp
    src/HealthEvaluator.h
    src/NullFocusProvider.cpp
    src/NullFocusProvider.h
    src/XdotoolFocusProvider.cpp
    src/XdotoolFocusProvider.h
    src/HyprctlFocusProvider.cpp
    src/HyprctlFocusProvider.h
    src/SwaymsgFocusProvider.cpp
    src/SwaymsgFocusProvider.h
    src/NiriFocusProvider.cpp
    src/NiriFocusProvider.h
    src/FocusProviderFactory.cpp
    src/FocusProviderFactory.h
    src/CachingFocusProvider.cpp
    src/CachingFocusProvider.h
    src/KdeFocusState.cpp
    src/KdeFocusState.h
    src/KdeFocusProvider.cpp
    src/KdeFocusProvider.h
    src/KdeFocusService.cpp
    src/KdeFocusService.h
    src/DockerStatus.cpp
    src/DockerStatus.h
    src/DockerSource.cpp
    src/DockerSource.h
    src/Config.cpp
    src/Config.h
    src/KubeStatus.cpp
    src/KubeStatus.h
    src/KubeSource.cpp
    src/KubeSource.h)

target_include_directories(debug_overlay_core PUBLIC src)
target_link_libraries(debug_overlay_core PUBLIC Qt6::Widgets Qt6::DBus)
target_compile_options(debug_overlay_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)

add_executable(debug_overlay_hud src/main.cpp)
target_link_libraries(debug_overlay_hud PRIVATE debug_overlay_core)

enable_testing()

add_executable(debug_overlay_tests tests/test_hud_buttons.cpp)
target_link_libraries(debug_overlay_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_focus_tests tests/test_focus.cpp)
target_link_libraries(debug_overlay_focus_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_cpu_tests tests/test_cpu.cpp)
target_link_libraries(debug_overlay_cpu_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_memory_tests tests/test_memory.cpp)
target_link_libraries(debug_overlay_memory_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_group_tests tests/test_group.cpp)
target_link_libraries(debug_overlay_group_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_sampler_tests tests/test_sampler.cpp)
target_link_libraries(debug_overlay_sampler_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_health_tests tests/test_health.cpp)
target_link_libraries(debug_overlay_health_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_provider_tests tests/test_providers.cpp)
target_link_libraries(debug_overlay_provider_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_docker_tests tests/test_docker.cpp)
target_link_libraries(debug_overlay_docker_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_config_tests tests/test_config.cpp)
target_link_libraries(debug_overlay_config_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_kube_tests tests/test_kube.cpp)
target_link_libraries(debug_overlay_kube_tests PRIVATE debug_overlay_core)

add_executable(debug_overlay_adapter_tests tests/test_adapters.cpp)
target_link_libraries(debug_overlay_adapter_tests PRIVATE debug_overlay_core)

add_test(NAME hud_buttons COMMAND debug_overlay_tests)
add_test(NAME focus COMMAND debug_overlay_focus_tests)
add_test(NAME cpu COMMAND debug_overlay_cpu_tests)
add_test(NAME memory COMMAND debug_overlay_memory_tests)
add_test(NAME group COMMAND debug_overlay_group_tests)
add_test(NAME sampler COMMAND debug_overlay_sampler_tests)
add_test(NAME health COMMAND debug_overlay_health_tests)
add_test(NAME providers COMMAND debug_overlay_provider_tests)
add_test(NAME docker COMMAND debug_overlay_docker_tests)
add_test(NAME config COMMAND debug_overlay_config_tests)
add_test(NAME kube COMMAND debug_overlay_kube_tests)
add_test(NAME adapters COMMAND debug_overlay_adapter_tests)

set_tests_properties(hud_buttons focus cpu memory group sampler health providers docker config kube adapters PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
