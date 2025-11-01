# 启用测试
enable_testing()
set(TEST_ROOT_DIR "${CMAKE_SOURCE_DIR}/test")
set(TEST_EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/output/test_program)
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_test_by_dir.cmake) # 注册测试
include(${CMAKE_SOURCE_DIR}/test/script/auto_add_exec.cmake)
auto_add_exec("base")

auto_add_test_by_dir("vulkan")
