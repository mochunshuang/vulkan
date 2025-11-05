#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

// NOLINTBEGIN
int main()
{
    // 初始化
    VkInstance instance = VK_NULL_HANDLE;
    uint32_t deviceCount = 0;
    std::vector<VkPhysicalDevice> devices;

    // 1. 创建 VkInstance (简化版，实际使用时需要配置应用程序信息和验证层)
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        std::cerr << "无法创建 Vulkan 实例!" << std::endl;
        return -1;
    }

    // 2. 枚举物理设备
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        std::cerr << "未找到支持 Vulkan 的 GPU!" << std::endl;
        vkDestroyInstance(instance, nullptr);
        return -1;
    }

    devices.resize(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // 3. 遍历每个设备并打印版本信息
    std::cout << "找到 " << deviceCount << " 个 Vulkan 设备:" << std::endl;
    std::cout << "==========================================" << std::endl;

    for (size_t i = 0; i < devices.size(); i++)
    {
        VkPhysicalDeviceProperties deviceProperties;
        // 获取设备属性（包含版本信息）
        vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);

        std::cout << "设备 " << i << ": " << deviceProperties.deviceName << std::endl;

        // 提取版本号组件
        uint32_t apiVersion = deviceProperties.apiVersion;
        uint32_t driverVersion = deviceProperties.driverVersion;

        // 解析版本号（Vulkan 版本号编码方式）
        uint32_t apiMajor = VK_VERSION_MAJOR(apiVersion);
        uint32_t apiMinor = VK_VERSION_MINOR(apiVersion);
        uint32_t apiPatch = VK_VERSION_PATCH(apiVersion);

        std::cout << "  - API 版本:    " << apiMajor << "." << apiMinor << "." << apiPatch
                  << std::endl;
        std::cout << "  - 驱动版本:    " << driverVersion;

        // 根据厂商提供更友好的驱动版本显示
        std::cout << " (原始值: 0x" << std::hex << driverVersion << std::dec << ")"
                  << std::endl;

        // 设备类型
        const char *deviceType = "";
        switch (deviceProperties.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            deviceType = "集成显卡";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            deviceType = "独立显卡";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            deviceType = "虚拟GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            deviceType = "CPU";
            break;
        default:
            deviceType = "其他";
        }
        std::cout << "  - 设备类型:    " << deviceType << std::endl;

        // 供应商ID
        std::cout << "  - 供应商ID:    0x" << std::hex << deviceProperties.vendorID
                  << std::dec << std::endl;

        std::cout << std::endl;
    }

    // 4. 清理
    vkDestroyInstance(instance, nullptr);

    return 0;
}
// NOLINTEND