cmake_minimum_required(VERSION 4.2.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MODULE_ENABLE ON CACHE BOOL "config enable moudle")
set(MODULE_ENABLE OFF) # 还没有成熟，只使用clang这个编译器

if(MODULE_ENABLE)
  add_compile_definitions(-DMODULE_ENABLE=1)

  # First enable module
  # https://github.com/Kitware/CMake/blob/v4.2.0-rc2/Help/dev/experimental.rst
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
  set(CMAKE_CXX_MODULE_STD ON)
else()
  add_compile_definitions(-DMODULE_ENABLE=0)
endif()

# Set language version used
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

project("example"
  LANGUAGES CXX
)

if(MODULE_ENABLE)
  add_executable(main)
  target_sources(
    main
    PUBLIC
    src/main.cpp
    PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
    src/math.cppm
    src/foo.cppm
  )
  add_executable(my_test)
  target_sources(my_test PUBLIC
    src/test.cpp)
else()
  add_executable(main)
  target_sources(main PUBLIC
    src/main2.cpp)
endif()

include(cmake/vulkan_modules.cmake)
add_subdirectory(vulkan)
add_subdirectory(third_party)

# 添加测试
#
include(${CMAKE_SOURCE_DIR}/test/CMakeLists.cmake)

get_target_property(glfw_include_dirs glfw INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "GLFW include dirs: ${glfw_include_dirs}")

macro(cppm_target target_name)
  add_executable(${target_name})
  target_sources(
    ${target_name}
    PUBLIC
    "test/cppm/${target_name}.cpp"
  )
  target_include_directories(${target_name} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw-3.3-stable/include"
  )
  target_compile_features(${target_name} PRIVATE cxx_std_23)
  target_link_libraries(${target_name} PRIVATE
    mcs_vulkan glm_modules glfw stb tinyobjloader tinygltf ktx
  )
  set_target_properties(${target_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan"
  )
endmacro()

# 定义一个宏，用于批量添加多个test_*目标
# 自动检测所有test/cppm/test_*.cpp文件并创建对应target
function(auto_add_cppm_target)
  # 扫描test/cppm目录下的所有test_*.cpp文件
  file(GLOB TEST_SOURCES "test/cppm/test_*.cpp")

  foreach(test_source IN LISTS TEST_SOURCES)
    # 从文件路径中提取target名称
    get_filename_component(FILENAME ${test_source} NAME_WE)

    # 创建对应的target
    cppm_target(${FILENAME})

    message(STATUS "auto_add_cppm_target: ${FILENAME}")
  endforeach()
endfunction()

if(MODULE_ENABLE)
  cppm_target(first_my_vulkan)
  cppm_target(GameObject)
  cppm_target(ui_base)
  cppm_target(point)
  cppm_target(modern)
  cppm_target(widget)
  cppm_target(hello_triangle)
  cppm_target(hello_triangle2)

  # 很难使用
  # cppm_target(hello_triangle3)
  # target_compile_definitions(hello_triangle3 PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
  cppm_target(hello_triangle4)
  cppm_target(type_test)

  # 使用方式：
  auto_add_cppm_target() # NOTE: cppm 和 vulkan 生成exe目录是一致的。仅仅分离shaders。兼容性
  auto_compile_slang_shaders(
    all_cppm_target_shaders
    ${CMAKE_SOURCE_DIR}/test/cppm/shaders
    ${TEST_EXECUTABLE_OUTPUT_PATH}/vulkan/shaders
  )
  add_dependencies(compile_all_vulkan_shaders all_cppm_target_shaders)
endif()