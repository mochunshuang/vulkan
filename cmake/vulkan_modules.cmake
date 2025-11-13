set(VULKAN_SDK_DIR "E:/mysoftware/VulkanSDK/1.4.321.1" CACHE PATH "Path to Vulkan SDK installation")

# NOTE:别装了 std.cppm 都是 static 的
add_library(vulkan_modules STATIC)
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
target_compile_features(vulkan_modules PRIVATE cxx_std_23)

# config vulkan
target_compile_definitions(vulkan_modules PUBLIC -DUSE_CPP20_MODULES -DVULKAN_HPP_NO_STRUCT_CONSTRUCTORS)

# for <cstdlib>
target_compile_definitions(vulkan_modules PUBLIC EXIT_SUCCESS=0 EXIT_FAILURE=1)
