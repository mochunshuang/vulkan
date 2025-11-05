set(TINYOBJLOADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/dependency/tinyobjloader")
set(TINYOBJLOADER_BUILD_DIR "${CMAKE_SOURCE_DIR}/depend_build/tinyobjloader")
add_subdirectory(${TINYOBJLOADER_SOURCE_DIR} ${TINYOBJLOADER_BUILD_DIR})

add_library(tinyobjloader_dep ALIAS tinyobjloader)
