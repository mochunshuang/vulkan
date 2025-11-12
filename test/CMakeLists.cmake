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
    "E:/0_github_project/vulkan_module/test/base/base.cpp"
)

# target_compile_features(base PRIVATE cxx_std_23)
# target_sources(base
# PUBLIC
# FILE_SET CXX_MODULES
# BASE_DIRS ${VULKAN_SDK_DIR}
# FILES
# "${VULKAN_SDK_DIR}/vulkan/vulkan.cppm"
# )
# target_include_directories(base
# PUBLIC
# "${VULKAN_SDK_DIR}"
# )

# 然后单独添加模块文件集
# add_library(vulkan_modules OBJECT)
# target_sources(vulkan_modules
# PUBLIC
# FILE_SET CXX_MODULES
# BASE_DIRS ${VULKAN_SDK_DIR}
# FILES
# "${VULKAN_SDK_DIR}/vulkan/vulkan.cppm"
# )
# target_compile_features(vulkan_modules PRIVATE cxx_std_23)
# target_include_directories(vulkan_modules
# PUBLIC
# "${VULKAN_SDK_DIR}"
# )
target_link_libraries(base vulkan_modules)