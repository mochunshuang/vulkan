cmake_minimum_required(VERSION 4.2.0)

# First enable module
# https://github.com/Kitware/CMake/blob/v4.2.0-rc2/Help/dev/experimental.rst
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set language version used
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

project("example"
  LANGUAGES CXX
)

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

include(cmake/third_party.cmake)

# 添加测试
#
include(${CMAKE_SOURCE_DIR}/test/CMakeLists.cmake)
