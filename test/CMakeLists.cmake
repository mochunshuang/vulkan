# 启用测试
enable_testing()
set(TEST_ROOT_DIR "${CMAKE_SOURCE_DIR}/test")
set(TEST_EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/output/test_program)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_exec.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_vulkan_module.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_compile_slang_shaders.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_compile_glslc_shaders.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_ray_tracing.cmake)

# 首先添加普通源文件
add_executable(base)
target_sources(
    base
    PRIVATE
    "test/base/base.cpp"
)
target_link_libraries(base PRIVATE vulkan_modules)

auto_add_vulkan_module("vulkan")
auto_compile_slang_shaders(
    compile_all_vulkan_shaders
    ${CMAKE_SOURCE_DIR}/test/vulkan/shaders
    ${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan/shaders
)
file(COPY ${CMAKE_SOURCE_DIR}/test/vulkan/textures
    DESTINATION ${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan
    FILES_MATCHING PATTERN "*")
file(COPY ${CMAKE_SOURCE_DIR}/test/vulkan/models
    DESTINATION ${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan
    FILES_MATCHING PATTERN "*")

set(COMP_ENTRY_POINTS -entry vertMain -entry fragMain -entry compMain)
add_compile_slang_shader(
    31_shader_compute_replace
    "${CMAKE_SOURCE_DIR}/test/vulkan/shaders/31_shader_compute.slang"
    "${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan/shaders/31_shader_compute_replace.spv"
    "${COMP_ENTRY_POINTS}"
)
add_compile_slang_shader(
    37_shader_compute_replace
    "${CMAKE_SOURCE_DIR}/test/vulkan/shaders/37_shader_compute.slang"
    "${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan/shaders/37_shader_compute_replace.spv"
    "${COMP_ENTRY_POINTS}"
)
auto_compile_vert_shaders(compile_all_vulkan_vert
    ${CMAKE_SOURCE_DIR}/test/vulkan/shaders
    ${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan/shaders)
auto_compile_frag_shaders(compile_all_vulkan_frag
    ${CMAKE_SOURCE_DIR}/test/vulkan/shaders
    ${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan/shaders)

auto_add_ray_tracing("ray_tracing/one" "glm_modules")