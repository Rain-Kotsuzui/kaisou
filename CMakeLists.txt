cmake_minimum_required(VERSION 3.25)

project(
    kaisou
    VERSION 0.1.0
    DESCRIPTION "kaisou Vulkan renderer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ENGINE_BUILD_MODE "Debug" CACHE STRING "Engine build mode label")
set_property(CACHE ENGINE_BUILD_MODE PROPERTY STRINGS Debug Daily Release)

option(ENGINE_ENABLE_LOGGING "Enable runtime logging" ON)
option(ENGINE_ENABLE_PROFILING "Enable CPU/GPU profiling hooks" OFF)
option(ENGINE_ENABLE_VALIDATION "Enable Vulkan validation by default" ON)
option(ENGINE_ENABLE_REFLECTION "Enable runtime and shader reflection metadata" ON)
option(ENGINE_ENABLE_ASSERTS "Enable engine assertions" ON)
option(ENGINE_ENABLE_GPU_DEBUG "Enable GPU object names and command labels" OFF)
option(ENGINE_BUILD_M1_DEMO "Build the archived M1 demo" OFF)

set(_engine_valid_modes Debug Daily Release)
if(NOT ENGINE_BUILD_MODE IN_LIST _engine_valid_modes)
    message(FATAL_ERROR "ENGINE_BUILD_MODE must be Debug, Daily, or Release")
endif()

if(ENGINE_ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
endif()

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
set(KAISOU_ENGINE_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/include"
)

# Core
add_library(kaisou_core STATIC)

target_sources(kaisou_core
    PRIVATE
        engine/src/core/log.cpp
        engine/src/core/assert.cpp
        engine/src/core/filesystem.cpp
        engine/src/core/hash.cpp
        engine/src/core/frame_limiter.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS
            "${KAISOU_ENGINE_INCLUDE_DIR}"
        FILES
            engine/include/kaisou/core/assert.hpp
            engine/include/kaisou/core/filesystem.hpp
            engine/include/kaisou/core/frame_limiter.hpp
            engine/include/kaisou/core/hash.hpp
            engine/include/kaisou/core/log.hpp
            engine/include/kaisou/core/profile.hpp
            engine/include/kaisou/core/time.hpp
            engine/include/kaisou/core/types.hpp
)

target_compile_definitions(kaisou_core
    PUBLIC
        KAISOU_ENABLE_LOGGING=$<BOOL:${ENGINE_ENABLE_LOGGING}>
        KAISOU_ENABLE_ASSERTS=$<BOOL:${ENGINE_ENABLE_ASSERTS}>
        KAISOU_ENABLE_PROFILING=$<BOOL:${ENGINE_ENABLE_PROFILING}>
)

target_link_libraries(kaisou_core
    PUBLIC
        fmt::fmt
    PRIVATE
        spdlog::spdlog
        xxHash::xxhash
)

if(ENGINE_ENABLE_PROFILING)
    target_link_libraries(kaisou_core
        PUBLIC
            Tracy::TracyClient
    )
endif()

# Platform
add_library(kaisou_platform STATIC)

target_sources(kaisou_platform
    PRIVATE
        engine/src/platform/window.cpp
        engine/src/platform/platform.cpp
        engine/src/platform/input.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS
            "${KAISOU_ENGINE_INCLUDE_DIR}"
        FILES
            engine/include/kaisou/platform/input.hpp
            engine/include/kaisou/platform/platform.hpp
            engine/include/kaisou/platform/window.hpp
)

target_link_libraries(kaisou_platform
    PUBLIC
        kaisou_core
    PRIVATE
        SDL3::SDL3
)

# Engine
add_library(kaisou_engine STATIC)

target_sources(kaisou_engine
    PRIVATE
        engine/src/application.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS
            "${KAISOU_ENGINE_INCLUDE_DIR}"
        FILES
            engine/include/kaisou/application.hpp
)

target_link_libraries(kaisou_engine
    PUBLIC
        kaisou_core
        kaisou_platform
)

# Vulkan backend
add_library(kaisou_rhi_vulkan STATIC)

target_sources(kaisou_rhi_vulkan
    PRIVATE
        engine/src/rhi/vulkan/instance.cpp
        engine/src/rhi/vulkan/device.cpp
        engine/src/rhi/vulkan/timeline_semaphore.cpp
        engine/src/rhi/vulkan/presentation/window_surface.cpp
        engine/src/rhi/vulkan/presentation/swapchain.cpp
        engine/src/rhi/vulkan/presentation/frame_resources.cpp
        engine/src/rhi/vulkan/presentation/presentation_context.cpp
        engine/src/rhi/vulkan/diagnostics/debug.cpp
        engine/src/rhi/vulkan/diagnostics/gpu_profiler.cpp
        engine/src/rhi/vulkan/diagnostics/gpu_trace.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS
            "${KAISOU_ENGINE_INCLUDE_DIR}"
        FILES
            engine/include/kaisou/rhi/vulkan/vulkan.hpp
            engine/include/kaisou/rhi/vulkan/instance.hpp
            engine/include/kaisou/rhi/vulkan/device.hpp
            engine/include/kaisou/rhi/vulkan/timeline_semaphore.hpp
            engine/include/kaisou/rhi/vulkan/presentation/window_surface.hpp
            engine/include/kaisou/rhi/vulkan/presentation/swapchain.hpp
            engine/include/kaisou/rhi/vulkan/presentation/frame_resources.hpp
            engine/include/kaisou/rhi/vulkan/presentation/presentation_context.hpp
            engine/include/kaisou/rhi/vulkan/diagnostics/debug.hpp
            engine/include/kaisou/rhi/vulkan/diagnostics/gpu_profiler.hpp
            engine/include/kaisou/rhi/vulkan/diagnostics/gpu_trace.hpp
)
target_compile_definitions(kaisou_rhi_vulkan
    PUBLIC
        KAISOU_ENABLE_VALIDATION=$<BOOL:${ENGINE_ENABLE_VALIDATION}>
    PRIVATE
        KAISOU_ENABLE_GPU_DEBUG=$<BOOL:${ENGINE_ENABLE_GPU_DEBUG}>
        KAISOU_ENGINE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        KAISOU_ENGINE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        KAISOU_ENGINE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
target_link_libraries(kaisou_rhi_vulkan
    PUBLIC
        volk::volk_headers
        kaisou_core
    PRIVATE
        kaisou_platform
        SDL3::SDL3
        volk::volk
        vk-bootstrap::vk-bootstrap
)
# Application
add_executable(kaisou)

target_sources(kaisou
    PRIVATE
        app/main.cpp
)

target_link_libraries(kaisou
    PRIVATE
        kaisou_engine
)

set_target_properties(kaisou PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)


if(ENGINE_BUILD_M1_DEMO)
    add_subdirectory(samples/m1)
endif()


message(STATUS "kaisou configuration")
message(STATUS "  Build mode       : ${ENGINE_BUILD_MODE}")
message(STATUS "  C++ compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Logging          : ${ENGINE_ENABLE_LOGGING}")
message(STATUS "  Profiling        : ${ENGINE_ENABLE_PROFILING}")
message(STATUS "  Validation       : ${ENGINE_ENABLE_VALIDATION}")
message(STATUS "  GPU debug        : ${ENGINE_ENABLE_GPU_DEBUG}")
message(STATUS "  Reflection       : ${ENGINE_ENABLE_REFLECTION}")
message(STATUS "  Assertions       : ${ENGINE_ENABLE_ASSERTS}")
message(STATUS "  M1 demo          : ${ENGINE_BUILD_M1_DEMO}")
