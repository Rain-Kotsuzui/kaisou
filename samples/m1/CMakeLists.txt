# Archived M1 demo: these targets never participate in the production app's dependency graph.
add_library(kaisou_m1_demo_renderer STATIC)

target_sources(kaisou_m1_demo_renderer
    PRIVATE
        src/clear_renderer.cpp
        src/triangle_pipeline.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
        FILES
            include/kaisou/samples/m1/clear_renderer.hpp
            include/kaisou/samples/m1/triangle_pipeline.hpp
)

target_compile_definitions(kaisou_m1_demo_renderer
    PRIVATE KAISOU_ENABLE_GPU_DEBUG=$<BOOL:${ENGINE_ENABLE_GPU_DEBUG}>
)

target_link_libraries(kaisou_m1_demo_renderer
    PUBLIC kaisou_core kaisou_rhi_vulkan
    PRIVATE kaisou_platform
)

add_executable(kaisou_m1_demo)
target_sources(kaisou_m1_demo PRIVATE main.cpp)
target_link_libraries(kaisou_m1_demo PRIVATE kaisou_engine kaisou_m1_demo_renderer)
set_target_properties(kaisou_m1_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

add_custom_target(kaisou_m1_demo_shaders ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:kaisou_m1_demo>/assets/shaders/m1"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.spv"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag.spv"
        "$<TARGET_FILE_DIR:kaisou_m1_demo>/assets/shaders/m1"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.spv"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag.spv"
    COMMENT "Deploy archived M1 demo shaders"
    VERBATIM
)
add_dependencies(kaisou_m1_demo kaisou_m1_demo_shaders)
