set(VULKAN_ROOT_PATH "E:/mysoftware/VulkanSDK/1.4.321.1")

# 创建导入目标
add_library(vulkan_dep STATIC IMPORTED)

# 设置目标属性
set_target_properties(vulkan_dep PROPERTIES
    IMPORTED_LOCATION "${VULKAN_ROOT_PATH}/Lib/vulkan-1.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_ROOT_PATH}/Include"
)