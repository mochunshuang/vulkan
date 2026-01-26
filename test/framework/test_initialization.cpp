

#include <iostream>
#include <vulkan/vulkan_core.h>
#include <vector>

// NOLINTBEGIN
// NOTE: 调用顺序很重要：总是按 版本 → 层 → 扩展 的顺序进行探测
// NOTE: 这些都是探测函数
void apiVersion()
{
    uint32_t apiVersion = 0;
    VkResult result = vkEnumerateInstanceVersion(&apiVersion);

    if (result != VK_SUCCESS)
    {
        // 系统可能完全没有Vulkan支持
        throw std::runtime_error("Failed to query Vulkan version");
    }

    // 解析版本号（Vulkan使用打包的版本号）
    uint32_t major = VK_VERSION_MAJOR(apiVersion);
    uint32_t minor = VK_VERSION_MINOR(apiVersion);
    uint32_t patch = VK_VERSION_PATCH(apiVersion);

    std::cout << "Available Vulkan version: " << major << "." << minor << "." << patch
              << std::endl;

    // 检查是否满足最低要求（如Vulkan 1.1）
    const uint32_t requiredVersion = VK_MAKE_VERSION(1, 1, 0);
    if (apiVersion < requiredVersion)
    {
        throw std::runtime_error("Vulkan version too old");
    }
}
void instanceLayerProperties()
{
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // 查看所有可用层
    std::cout << "Available instance layers:" << std::endl;
    for (const auto &layer : availableLayers)
    {
        std::cout << "  " << layer.layerName
                  << " (spec: " << VK_VERSION_MAJOR(layer.specVersion) << "."
                  << VK_VERSION_MINOR(layer.specVersion) << ")" << std::endl;
    }

    // 检查是否支持验证层
    const char *validationLayerName = "VK_LAYER_KHRONOS_validation";
    bool validationLayerAvailable = false;

    for (const auto &layer : availableLayers)
    {
        if (strcmp(layer.layerName, validationLayerName) == 0)
        {
            validationLayerAvailable = true;
            break;
        }
    }

    // 准备在创建实例时启用验证层
    std::vector<const char *> enabledLayers;
    if (validationLayerAvailable)
    {
        enabledLayers.push_back(validationLayerName);
        std::cout << "Validation layer enabled" << std::endl;
    }
}
void instanceExtensionPropertie()
{
    // 枚举全局扩展（不属于任何层）
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                           availableExtensions.data());

    // 查看全局扩展
    std::cout << "Available instance extensions:" << std::endl;
    for (const auto &extension : availableExtensions)
    {
        std::cout << "  " << extension.extensionName
                  << " (version: " << extension.specVersion << ")" << std::endl;
    }

    // 检查特定扩展（如调试扩展）
    const char *debugExtensionName = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    bool debugExtensionAvailable = false;

    for (const auto &extension : availableExtensions)
    {
        if (strcmp(extension.extensionName, debugExtensionName) == 0)
        {
            debugExtensionAvailable = true;
            break;
        }
    }

    // 枚举特定层提供的扩展
    const char *layerName = "VK_LAYER_KHRONOS_validation";
    uint32_t layerExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(layerName, &layerExtensionCount, nullptr);

    std::vector<VkExtensionProperties> layerExtensions(layerExtensionCount);
    vkEnumerateInstanceExtensionProperties(layerName, &layerExtensionCount,
                                           layerExtensions.data());

    // 准备在创建实例时启用的扩展列表
    std::vector<const char *> enabledExtensions;
    enabledExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME); // 表面支持

#ifdef _WIN32
    // enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME); // Win32表面
#endif

    if (debugExtensionAvailable)
    {
        enabledExtensions.push_back(debugExtensionName); // 调试工具
    }
}
int main()
{
    apiVersion();
    instanceLayerProperties();
    instanceExtensionPropertie();
    std::cout << "main done\n";
    return 0;
}
// NOLINTEND