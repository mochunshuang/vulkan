

set(VULKAN_SDK_DIR "E:/mysoftware/VulkanSDK/1.4.321.1")
add_library(vulkan_modules OBJECT)
target_sources(vulkan_modules
    PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${VULKAN_SDK_DIR}
    FILES
    "${VULKAN_SDK_DIR}/Include/vulkan/vulkan.cppm"
)
target_include_directories(vulkan_modules
    PUBLIC
    "${VULKAN_SDK_DIR}/Include"
)

# config vulkan
target_compile_definitions(vulkan_modules PUBLIC -DUSE_CPP20_MODULES -DVULKAN_HPP_NO_STRUCT_CONSTRUCTORS)

# for <cstdlib>
target_compile_definitions(vulkan_modules PUBLIC EXIT_SUCCESS=0 EXIT_FAILURE=1)

unset(VULKAN_SDK_DIR)
