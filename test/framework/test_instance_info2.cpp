// NOLINTBEGIN
#include "./head.hpp"
#include <vulkan/vulkan.hpp> // 确保包含 Vulkan-Hpp
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

using surface = mcs::vulkan::wsi::glfw::Window;
using make_instance = mcs::vulkan::make_instance;
using make_physical_device = mcs::vulkan::make_physical_device;
using physical_device = mcs::vulkan::physical_device;
using make_logical_device = mcs::vulkan::make_logical_device;
using mcs::vulkan::structure_chain;
using mcs::vulkan::sType;

// 使用 Vulkan-Hpp 命名空间简化代码

template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

// ==================== 优化的辅助函数 ====================

// 使用 Vulkan-Hpp 的内置转换
std::string flagsToString(vk::Flags<vk::QueueFlagBits> flags)
{
    if (flags == vk::QueueFlagBits::eGraphics)
        return "图形";
    if (flags == vk::QueueFlagBits::eCompute)
        return "计算";
    if (flags == vk::QueueFlagBits::eTransfer)
        return "传输";

    std::string result;
    if (flags & vk::QueueFlagBits::eGraphics)
        result += "图形 ";
    if (flags & vk::QueueFlagBits::eCompute)
        result += "计算 ";
    if (flags & vk::QueueFlagBits::eTransfer)
        result += "传输 ";
    if (flags & vk::QueueFlagBits::eSparseBinding)
        result += "稀疏绑定 ";
    if (flags & vk::QueueFlagBits::eProtected)
        result += "保护 ";

    if (!result.empty())
        result.pop_back(); // 移除最后一个空格
    return result.empty() ? "无" : result;
}

// 通用的位标志转字符串函数
template <typename FlagBits>
std::string flagsToString(typename vk::Flags<FlagBits> flags,
                          std::function<std::string(FlagBits)> toStringFunc)
{
    if (flags == static_cast<FlagBits>(0))
        return "无";

    std::string result;
    for (int i = 0; i < 32; ++i)
    {
        FlagBits bit = static_cast<FlagBits>(1 << i);
        if (flags & bit)
        {
            if (!result.empty())
                result += " | ";
            result += toStringFunc(bit);
        }
    }
    return result;
}

void printSeparator(const std::string &title)
{
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// ==================== 优化的查询函数 ====================

void queryInstanceInfo()
{
    printSeparator("实例信息查询");

    // 1. Vulkan API版本
    uint32_t apiVersion = 0;
    if (vk::enumerateInstanceVersion(&apiVersion) == vk::Result::eSuccess)
    {
        std::cout << "支持的Vulkan实例级API版本: " << vk::versionMajor(apiVersion) << "."
                  << vk::versionMinor(apiVersion) << "." << vk::versionPatch(apiVersion)
                  << "\n";
    }

    // 2. 实例扩展 - 使用异常安全的方式
    try
    {
        auto instanceExtensions = vk::enumerateInstanceExtensionProperties();

        std::cout << "\n实例扩展 (" << instanceExtensions.size() << "个):\n";
        for (const auto &ext : instanceExtensions)
        {
            std::cout << "  - " << std::left << std::setw(50) << ext.extensionName
                      << " 版本: " << ext.specVersion << "\n";
        }
    }
    catch (const vk::SystemError &e)
    {
        std::cout << "查询实例扩展失败: " << e.what() << "\n";
    }

    // 3. 验证层
    try
    {
        auto layers = vk::enumerateInstanceLayerProperties();

        std::cout << "\n验证层 (" << layers.size() << "个):\n";
        for (const auto &layer : layers)
        {
            std::cout << "  - " << std::left << std::setw(40) << layer.layerName << " "
                      << layer.description << "\n";
        }
    }
    catch (const vk::SystemError &e)
    {
        std::cout << "查询验证层失败: " << e.what() << "\n";
    }
}

void queryPhysicalDeviceInfo(const vk::Instance &instance, vk::SurfaceKHR surface)
{
    printSeparator("物理设备信息查询");

    // 枚举所有物理设备
    std::vector<vk::PhysicalDevice> physicalDevices;
    try
    {
        physicalDevices = instance.enumeratePhysicalDevices();
    }
    catch (const vk::SystemError &e)
    {
        std::cout << "枚举物理设备失败: " << e.what() << "\n";
        return;
    }

    if (physicalDevices.empty())
    {
        std::cout << "未找到支持Vulkan的物理设备！\n";
        return;
    }

    for (size_t i = 0; i < physicalDevices.size(); ++i)
    {
        std::cout << "\n--- 物理设备 [" << i << "] ---\n";

        vk::PhysicalDevice physDevice = physicalDevices[i];

        try
        {
            // 1. 设备属性
            vk::PhysicalDeviceProperties props = physDevice.getProperties();
            std::cout << "设备名称: " << props.deviceName << "\n";
            std::cout << "设备类型: " << vk::to_string(props.deviceType) << "\n";
            std::cout << "API版本: " << vk::versionMajor(props.apiVersion) << "."
                      << vk::versionMinor(props.apiVersion) << "."
                      << vk::versionPatch(props.apiVersion) << "\n";
            std::cout << "驱动版本: " << props.driverVersion << "\n";
            std::cout << "供应商ID: 0x" << std::hex << props.vendorID << std::dec << "\n";
            std::cout << "设备ID: 0x" << std::hex << props.deviceID << std::dec << "\n";

            // 2. 设备特性
            vk::PhysicalDeviceFeatures features = physDevice.getFeatures();
            std::cout << "\n设备特性:\n";
            std::cout << "  几何着色器: " << (features.geometryShader ? "支持" : "不支持")
                      << "\n";
            std::cout << "  曲面细分着色器: "
                      << (features.tessellationShader ? "支持" : "不支持") << "\n";
            std::cout << "  各向异性过滤: "
                      << (features.samplerAnisotropy ? "支持" : "不支持") << "\n";

            // 3. 设备扩展
            auto deviceExtensions = physDevice.enumerateDeviceExtensionProperties();
            std::cout << "\n设备扩展 (" << deviceExtensions.size() << "个):\n";
            for (const auto &ext : deviceExtensions)
            {
                std::cout << "  - " << std::left << std::setw(60) << ext.extensionName
                          << " 版本: " << ext.specVersion << "\n";
            }

            // 4. 队列族信息
            auto queueFamilies = physDevice.getQueueFamilyProperties();
            std::cout << "\n队列族 (" << queueFamilies.size() << "个):\n";
            for (size_t j = 0; j < queueFamilies.size(); ++j)
            {
                const auto &qf = queueFamilies[j];
                std::cout << "  队列族[" << j << "]: " << qf.queueCount
                          << "个队列, 支持: " << flagsToString(qf.queueFlags) << "\n";

                // 检查表面支持
                VkBool32 surfaceSupported =
                    physDevice.getSurfaceSupportKHR(static_cast<uint32_t>(j), surface);
                std::cout << "      表面支持: " << (surfaceSupported ? "是" : "否")
                          << "\n";
            }

            // 5. 内存信息
            auto memoryProps = physDevice.getMemoryProperties();
            std::cout << "\n内存堆 (" << memoryProps.memoryHeapCount << "个):\n";
            for (uint32_t j = 0; j < memoryProps.memoryHeapCount; ++j)
            {
                std::cout << "  堆[" << j
                          << "]: " << memoryProps.memoryHeaps[j].size / (1024 * 1024)
                          << " MB";
                if (memoryProps.memoryHeaps[j].flags &
                    vk::MemoryHeapFlagBits::eDeviceLocal)
                    std::cout << " (设备本地)";
                std::cout << "\n";
            }

            // 6. 表面能力查询
            std::cout << "\n表面能力:\n";
            try
            {
                auto surfaceCapabilities = physDevice.getSurfaceCapabilitiesKHR(surface);
                std::cout << "  最小图像数: " << surfaceCapabilities.minImageCount
                          << "\n";
                std::cout << "  最大图像数: " << surfaceCapabilities.maxImageCount
                          << "\n";
                std::cout << "  当前范围: " << surfaceCapabilities.currentExtent.width
                          << "x" << surfaceCapabilities.currentExtent.height << "\n";
            }
            catch (const vk::SystemError &e)
            {
                std::cout << "  无法查询表面能力: " << e.what() << "\n";
            }

            // 7. 表面格式
            std::cout << "\n支持的表面格式:\n";
            try
            {
                auto surfaceFormats = physDevice.getSurfaceFormatsKHR(surface);
                for (const auto &format : surfaceFormats)
                {
                    std::cout << "  - 格式: " << vk::to_string(format.format)
                              << ", 颜色空间: " << vk::to_string(format.colorSpace)
                              << "\n";
                }
            }
            catch (const vk::SystemError &e)
            {
                std::cout << "  无法查询表面格式: " << e.what() << "\n";
            }

            // 8. 呈现模式
            std::cout << "\n支持的呈现模式:\n";
            try
            {
                auto presentModes = physDevice.getSurfacePresentModesKHR(surface);
                for (const auto &mode : presentModes)
                {
                    std::cout << "  - " << vk::to_string(mode) << "\n";
                }
            }
            catch (const vk::SystemError &e)
            {
                std::cout << "  无法查询呈现模式: " << e.what() << "\n";
            }
        }
        catch (const vk::SystemError &e)
        {
            std::cout << "查询设备信息失败: " << e.what() << "\n";
        }
    }
}

int main()
{
    try
    {
        // 1. 创建窗口（仅用于创建表面）
        surface window;
        window.setup({.width = 800, .height = 600}, "Vulkan信息查询工具");

        // 2. 查询实例信息
        queryInstanceInfo();

        // 3. 创建实例
        auto instance = make_instance{}
                            .enableDebugExtension()
                            .enableSurfaceExtension<surface>()
                            .checkExtensionSupport()
                            .checkLayerSupport()
                            .build({.sType = sType<VkApplicationInfo>(),
                                    .pApplicationName = "Vulkan信息查询",
                                    .applicationVersion = VkApiVersion(1, 0, 0),
                                    .pEngineName = "信息查询引擎",
                                    .engineVersion = VkApiVersion(1, 0, 0),
                                    .apiVersion = VkApiVersion(0, 1, 4, 0)});

        // 4. 创建表面
        VkSurfaceKHR surface = window.createVkSurfaceKHR(instance.ref_data());

        // 5. 查询物理设备信息（包括表面信息）
        queryPhysicalDeviceInfo(instance.ref_data(), surface);

        // 6. 清理资源
        mcs::vulkan::surface_extension::destroy(instance.ref_data(), surface);

        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "Vulkan信息查询完成！\n";
        std::cout << std::string(80, '=') << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "错误: " << e.what() << "\n";
        return -1;
    }

    return 0;
}
// NOLINTEND