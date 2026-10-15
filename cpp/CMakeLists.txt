cmake_minimum_required(VERSION 3.16)

project(syspeek
    VERSION 0.4.0
    DESCRIPTION "Syspeek HUD (Qt 6 Widgets)"
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

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus Network)

add_library(syspeek_core STATIC
    src/HudWindow.cpp
    src/HudWindow.h
    src/DashboardPanel.cpp
    src/DashboardPanel.h
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
    src/AlertEngine.cpp
    src/AlertEngine.h
    src/Notifier.cpp
    src/Notifier.h
    src/HostSampler.cpp
    src/HostSampler.h
    src/IoRateTracker.cpp
    src/IoRateTracker.h
    src/NetRateTracker.cpp
    src/NetRateTracker.h
    src/ServicesStatus.cpp
    src/ServicesStatus.h
    src/ServicesSource.cpp
    src/ServicesSource.h
    src/SyntheticStatus.cpp
    src/SyntheticStatus.h
    src/SyntheticSource.cpp
    src/SyntheticSource.h
    src/JournalStatus.cpp
    src/JournalStatus.h
    src/JournalSource.cpp
    src/JournalSource.h
    src/MemoryFormat.cpp
    src/MemoryFormat.h
    src/MetricStore.cpp
    src/MetricStore.h
    src/Sparkline.cpp
    src/Sparkline.h
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
    src/CostModel.cpp
    src/CostModel.h
    src/DiskSampler.cpp
    src/DiskSampler.h
    src/DiskStatus.cpp
    src/DiskStatus.h
    src/TcpSampler.cpp
    src/TcpSampler.h
    src/TcpStatus.cpp
    src/TcpStatus.h
    src/SecurityStatus.cpp
    src/SecurityStatus.h
    src/LogParser.cpp
    src/LogParser.h
    src/TraceSource.cpp
    src/TraceSource.h
    src/TraceStatus.cpp
    src/TraceStatus.h
    src/SbomSource.cpp
    src/SbomSource.h
    src/SbomStatus.cpp
    src/SbomStatus.h
    src/PathSource.cpp
    src/PathSource.h
    src/PathStatus.cpp
    src/PathStatus.h
    src/StatsdSource.cpp
    src/StatsdSource.h
    src/StatsdStatus.cpp
    src/StatsdStatus.h
    src/SensitiveStatus.cpp
    src/SensitiveStatus.h
    src/KubeStatus.cpp
    src/KubeStatus.h
    src/KubeSource.cpp
    src/KubeSource.h)

target_include_directories(syspeek_core PUBLIC src)
target_link_libraries(syspeek_core PUBLIC Qt6::Widgets Qt6::DBus Qt6::Network)
target_compile_options(syspeek_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)

add_executable(syspeek src/main.cpp)
target_link_libraries(syspeek PRIVATE syspeek_core)

enable_testing()

add_executable(syspeek_tests tests/test_hud_buttons.cpp)
target_link_libraries(syspeek_tests PRIVATE syspeek_core)

add_executable(syspeek_focus_tests tests/test_focus.cpp)
target_link_libraries(syspeek_focus_tests PRIVATE syspeek_core)

add_executable(syspeek_cpu_tests tests/test_cpu.cpp)
target_link_libraries(syspeek_cpu_tests PRIVATE syspeek_core)

add_executable(syspeek_memory_tests tests/test_memory.cpp)
target_link_libraries(syspeek_memory_tests PRIVATE syspeek_core)

add_executable(syspeek_group_tests tests/test_group.cpp)
target_link_libraries(syspeek_group_tests PRIVATE syspeek_core)

add_executable(syspeek_sampler_tests tests/test_sampler.cpp)
target_link_libraries(syspeek_sampler_tests PRIVATE syspeek_core)

add_executable(syspeek_health_tests tests/test_health.cpp)
target_link_libraries(syspeek_health_tests PRIVATE syspeek_core)

add_executable(syspeek_provider_tests tests/test_providers.cpp)
target_link_libraries(syspeek_provider_tests PRIVATE syspeek_core)

add_executable(syspeek_docker_tests tests/test_docker.cpp)
target_link_libraries(syspeek_docker_tests PRIVATE syspeek_core)

add_executable(syspeek_config_tests tests/test_config.cpp)
target_link_libraries(syspeek_config_tests PRIVATE syspeek_core)

add_executable(syspeek_kube_tests tests/test_kube.cpp)
target_link_libraries(syspeek_kube_tests PRIVATE syspeek_core)

add_executable(syspeek_adapter_tests tests/test_adapters.cpp)
target_link_libraries(syspeek_adapter_tests PRIVATE syspeek_core)

add_executable(syspeek_metrics_tests tests/test_metrics.cpp)
target_link_libraries(syspeek_metrics_tests PRIVATE syspeek_core)

add_executable(syspeek_screenshot EXCLUDE_FROM_ALL tools/screenshot.cpp)
target_link_libraries(syspeek_screenshot PRIVATE syspeek_core)

add_test(NAME hud_buttons COMMAND syspeek_tests)
add_test(NAME focus COMMAND syspeek_focus_tests)
add_test(NAME cpu COMMAND syspeek_cpu_tests)
add_test(NAME memory COMMAND syspeek_memory_tests)
add_test(NAME group COMMAND syspeek_group_tests)
add_test(NAME sampler COMMAND syspeek_sampler_tests)
add_test(NAME health COMMAND syspeek_health_tests)
add_test(NAME providers COMMAND syspeek_provider_tests)
add_test(NAME docker COMMAND syspeek_docker_tests)
add_test(NAME config COMMAND syspeek_config_tests)
add_test(NAME kube COMMAND syspeek_kube_tests)
add_test(NAME adapters COMMAND syspeek_adapter_tests)
add_test(NAME metrics COMMAND syspeek_metrics_tests)

set_tests_properties(hud_buttons focus cpu memory group sampler health providers docker config kube adapters metrics PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
