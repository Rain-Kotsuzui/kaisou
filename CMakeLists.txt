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

set(ENGINE_LOG_LEVEL "Trace" CACHE STRING "Compiled default log level")
set_property(CACHE ENGINE_LOG_LEVEL PROPERTY STRINGS Trace Debug Info Warn Error)

option(ENGINE_ENABLE_LOGGING "Enable runtime logging" ON)
option(ENGINE_ENABLE_PROFILING "Enable CPU/GPU profiling hooks" OFF)
option(ENGINE_ENABLE_VALIDATION "Enable Vulkan validation by default" ON)
option(ENGINE_ENABLE_REFLECTION "Enable runtime and shader reflection metadata" ON)
option(ENGINE_ENABLE_ASSERTS "Enable engine assertions" ON)

set(_engine_valid_modes Debug Daily Release)
if(NOT ENGINE_BUILD_MODE IN_LIST _engine_valid_modes)
    message(FATAL_ERROR "ENGINE_BUILD_MODE must be Debug, Daily, or Release")
endif()

message(STATUS "kaisou configuration")
message(STATUS "  Build mode       : ${ENGINE_BUILD_MODE}")
message(STATUS "  C++ compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Logging          : ${ENGINE_ENABLE_LOGGING} (${ENGINE_LOG_LEVEL})")
message(STATUS "  Profiling        : ${ENGINE_ENABLE_PROFILING}")
message(STATUS "  Validation       : ${ENGINE_ENABLE_VALIDATION}")
message(STATUS "  Reflection       : ${ENGINE_ENABLE_REFLECTION}")
message(STATUS "  Assertions       : ${ENGINE_ENABLE_ASSERTS}")
message(STATUS "  Targets          : none")
