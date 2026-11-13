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

set(_engine_valid_modes Debug Daily Release)
if(NOT ENGINE_BUILD_MODE IN_LIST _engine_valid_modes)
    message(FATAL_ERROR "ENGINE_BUILD_MODE must be Debug, Daily, or Release")
endif()

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_library(kaisou_core STATIC
    engine/src/core/log.cpp
)

target_include_directories(kaisou_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/include"
)

target_link_libraries(kaisou_core PUBLIC
    spdlog::spdlog
    fmt::fmt
)

target_compile_definitions(kaisou_core PUBLIC
    KAISOU_ENABLE_LOGGING=$<BOOL:${ENGINE_ENABLE_LOGGING}>
)

add_executable(kaisou
    app/main.cpp
)


target_link_libraries(kaisou PRIVATE
    kaisou_core
)


set_target_properties(kaisou PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)


message(STATUS "kaisou configuration")
message(STATUS "  Build mode       : ${ENGINE_BUILD_MODE}")
message(STATUS "  C++ compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Logging          : ${ENGINE_ENABLE_LOGGING}")
message(STATUS "  Profiling        : ${ENGINE_ENABLE_PROFILING}")
message(STATUS "  Validation       : ${ENGINE_ENABLE_VALIDATION}")
message(STATUS "  Reflection       : ${ENGINE_ENABLE_REFLECTION}")
message(STATUS "  Assertions       : ${ENGINE_ENABLE_ASSERTS}")
message(STATUS "  Targets          : kaisou")
