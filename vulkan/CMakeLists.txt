
add_library(mcs_vulkan STATIC)
target_sources(mcs_vulkan
    PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
    "utils/__utils.cppm"
    "api/__api.cppm"
    "mcs_vulkan.cppm"
)
target_compile_features(mcs_vulkan PRIVATE cxx_std_23)
target_link_libraries(mcs_vulkan PUBLIC vulkan_modules)