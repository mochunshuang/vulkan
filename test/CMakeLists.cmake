# 启用测试
enable_testing()
set(TEST_ROOT_DIR "${CMAKE_SOURCE_DIR}/test")
set(TEST_EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/output/test_program)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_exec.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_vulkan_module.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_compile_slang_shaders.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_compile_glslc_shaders.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_ray_tracing.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_framework.cmake)

if(MODULE_ENABLE)
    # 首先添加普通源文件
    add_executable(base)
    target_sources(
        base
        PRIVATE
        "test/base/base.cpp"
    )
    target_link_libraries(base PRIVATE vulkan_modules)

    auto_add_vulkan_module("vulkan")
endif()

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

# auto_add_ray_tracing("ray_tracing/one" "glm_modules")
auto_add_ray_tracing("ray_tracing/next" "stb")
file(COPY ${CMAKE_SOURCE_DIR}/test/ray_tracing/images
    DESTINATION ${TEST_EXECUTABLE_OUTPUT_PATH}/ray_tracing
    FILES_MATCHING PATTERN "*")

# my_vulkan
auto_add_vulkan_module("my_vulkan")

file(COPY ${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan/shaders
    DESTINATION ${TEST_EXECUTABLE_OUTPUT_PATH}/my_vulkan
    FILES_MATCHING PATTERN "*")
file(COPY ${CMAKE_SOURCE_DIR}/test/vulkan/textures
    DESTINATION ${TEST_EXECUTABLE_OUTPUT_PATH}/my_vulkan
    FILES_MATCHING PATTERN "*")
file(COPY ${CMAKE_SOURCE_DIR}/test/vulkan/models
    DESTINATION ${TEST_EXECUTABLE_OUTPUT_PATH}/my_vulkan
    FILES_MATCHING PATTERN "*")

#
auto_add_vulkan_module("raii")

auto_add_framework("framework/wsi")
auto_add_framework("framework")
file(COPY ${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan/shaders
    DESTINATION ${TEST_EXECUTABLE_OUTPUT_PATH}/framework
    FILES_MATCHING PATTERN "*")
file(COPY ${CMAKE_SOURCE_DIR}/test/vulkan/textures
    DESTINATION ${TEST_EXECUTABLE_OUTPUT_PATH}/framework
    FILES_MATCHING PATTERN "*")
file(COPY ${CMAKE_SOURCE_DIR}/test/vulkan/models
    DESTINATION ${TEST_EXECUTABLE_OUTPUT_PATH}/framework
    FILES_MATCHING PATTERN "*")
auto_compile_vert_shaders(compile_all_framework_vert
    ${CMAKE_SOURCE_DIR}/test/framework/shaders
    ${TEST_EXECUTABLE_OUTPUT_PATH}/framework/shaders)
auto_compile_frag_shaders(compile_all_framework_frag
    ${CMAKE_SOURCE_DIR}/test/framework/shaders
    ${TEST_EXECUTABLE_OUTPUT_PATH}/framework/shaders)

# no_add_test
include(${CMAKE_SOURCE_DIR}/test/no_add_test/no_add_test.cmake)

# -------------shadertoy-----------------------
set(DIR_NAME "shadertoy")
auto_add_framework(${DIR_NAME})
auto_compile_vert_shaders_with_prefix("compile_all_${DIR_NAME}_vert"
    ${CMAKE_SOURCE_DIR}/test/${DIR_NAME}/shaders
    ${TEST_EXECUTABLE_OUTPUT_PATH}/${DIR_NAME}/shaders ${DIR_NAME})
auto_compile_frag_shaders_with_prefix("compile_all_${DIR_NAME}_frag"
    ${CMAKE_SOURCE_DIR}/test/${DIR_NAME}/shaders
    ${TEST_EXECUTABLE_OUTPUT_PATH}/${DIR_NAME}/shaders ${DIR_NAME})

include(${CMAKE_SOURCE_DIR}/test/examples/examples.cmake)