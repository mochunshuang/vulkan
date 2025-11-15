# git submodule add https://gh-proxy.com/https://github.com/syoyo/tinygltf third_party/tinygltf

# tinygltf
add_library(tinygltf INTERFACE)
set(TINYGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf)
target_sources(tinygltf INTERFACE ${TINYGLTF_DIR}/tiny_gltf.h ${TINYGLTF_DIR}/json.hpp)
target_include_directories(tinygltf SYSTEM INTERFACE ${TINYGLTF_DIR})
