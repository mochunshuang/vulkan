# 启用测试
enable_testing()
set(TEST_ROOT_DIR "${CMAKE_SOURCE_DIR}/test")
set(TEST_EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/output/test_program)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_exec.cmake)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_vulkan_module.cmake)

# 首先添加普通源文件
add_executable(base)
target_sources(
    base
    PRIVATE
    "test/base/base.cpp"
)
target_link_libraries(base PRIVATE vulkan_modules)

auto_add_vulkan_module("vulkan")