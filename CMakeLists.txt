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
add_subdirectory(third_party)

# 添加测试
#
include(${CMAKE_SOURCE_DIR}/test/CMakeLists.cmake)
