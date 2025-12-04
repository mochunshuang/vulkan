cmake_minimum_required(VERSION 4.2.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MODULE_ENABLE ON CACHE BOOL "config enable moudle")

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

get_target_property(mcs_vulkan_include_dirs mcs_vulkan INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "mcs_vulkan include dirs: ${mcs_vulkan_include_dirs}")

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
cppm_target(test_0)
cppm_target(test_1)
cppm_target(test_2)