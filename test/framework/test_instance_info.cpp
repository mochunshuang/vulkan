#include "./head.hpp"

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

// NOLINTBEGIN
template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

// ==================== 辅助函数 ====================

std::string deviceTypeToString(VkPhysicalDeviceType type)
{
    switch (type)
    {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "集成GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "独立GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "虚拟GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        return "其他";
    default:
        return "未知";
    }
}

std::string formatToString(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
        return "R8G8B8A8_UNORM";
    case VK_FORMAT_B8G8R8A8_UNORM:
        return "B8G8R8A8_UNORM";
    case VK_FORMAT_B8G8R8A8_SRGB:
        return "B8G8R8A8_SRGB";
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return "R32G32B32A32_SFLOAT";
    case VK_FORMAT_D32_SFLOAT:
        return "D32_SFLOAT";
    default:
        return "未知格式(" + std::to_string(format) + ")";
    }
}

std::string colorSpaceToString(VkColorSpaceKHR colorSpace)
{
    switch (colorSpace)
    {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        return "sRGB非线性";
    case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
        return "DisplayP3非线性";
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        return "扩展sRGB线性";
    default:
        return "未知颜色空间(" + std::to_string(colorSpace) + ")";
    }
}

std::string presentModeToString(VkPresentModeKHR mode)
{
    switch (mode)
    {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "立即模式";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "邮箱模式";
    case VK_PRESENT_MODE_FIFO_KHR:
        return "FIFO模式";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        return "放松FIFO模式";
    case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
        return "共享需求刷新";
    case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
        return "共享连续刷新";
    default:
        return "未知模式(" + std::to_string(mode) + ")";
    }
}

void printSeparator(const std::string &title)
{
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// ==================== 查询函数 ====================

void queryInstanceInfo()
{
    printSeparator("实例信息查询");

    // 1. Vulkan API版本
    uint32_t apiVersion = 0;
    if (vkEnumerateInstanceVersion(&apiVersion) == VK_SUCCESS)
    {
        std::cout << "支持的Vulkan实例级API版本: " << VK_VERSION_MAJOR(apiVersion) << "."
                  << VK_VERSION_MINOR(apiVersion) << "." << VK_VERSION_PATCH(apiVersion)
                  << "\n";
    }

    // 2. 实例扩展
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> instanceExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                           instanceExtensions.data());

    std::cout << "\n实例扩展 (" << extensionCount << "个):\n";
    for (const auto &ext : instanceExtensions)
    {
        std::cout << "  - " << std::left << std::setw(50) << ext.extensionName
                  << " 版本: " << ext.specVersion << "\n";
    }

    // 3. 验证层
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    std::cout << "\n验证层 (" << layerCount << "个):\n";
    for (const auto &layer : layers)
    {
        std::cout << "  - " << std::left << std::setw(40) << layer.layerName << " "
                  << layer.description << "\n";
    }
}

void queryPhysicalDeviceInfo(const VkInstance &instance, VkSurfaceKHR surface)
{
    printSeparator("物理设备信息查询");

    // 枚举所有物理设备
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    if (physicalDeviceCount == 0)
    {
        std::cout << "未找到支持Vulkan的物理设备！\n";
        return;
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

    for (size_t i = 0; i < physicalDevices.size(); ++i)
    {
        std::cout << "\n--- 物理设备 [" << i << "] ---\n";

        VkPhysicalDevice physDevice = physicalDevices[i];
        physical_device deviceWrapper(physDevice);

        // 1. 设备属性
        auto props = deviceWrapper.getProperties();
        std::cout << "设备名称: " << props.deviceName << "\n";
        std::cout << "设备类型: " << deviceTypeToString(props.deviceType) << "\n";
        std::cout << "API版本: " << VK_VERSION_MAJOR(props.apiVersion) << "."
                  << VK_VERSION_MINOR(props.apiVersion) << "."
                  << VK_VERSION_PATCH(props.apiVersion) << "\n";
        std::cout << "驱动版本: " << props.driverVersion << "\n";
        std::cout << "供应商ID: 0x" << std::hex << props.vendorID << std::dec << "\n";
        std::cout << "设备ID: 0x" << std::hex << props.deviceID << std::dec << "\n";

        // 2. 设备特性
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physDevice, &features);
        std::cout << "\n设备特性:\n";
        std::cout << "  几何着色器: " << (features.geometryShader ? "支持" : "不支持")
                  << "\n";
        std::cout << "  曲面细分着色器: "
                  << (features.tessellationShader ? "支持" : "不支持") << "\n";
        std::cout << "  各向异性过滤: "
                  << (features.samplerAnisotropy ? "支持" : "不支持") << "\n";
        std::cout << "  纹理压缩ETC2: "
                  << (features.textureCompressionETC2 ? "支持" : "不支持") << "\n";
        std::cout << "  纹理压缩BC: "
                  << (features.textureCompressionBC ? "支持" : "不支持") << "\n";
        std::cout << "  多视口: " << (features.multiViewport ? "支持" : "不支持") << "\n";

        // 3. 设备扩展
        auto deviceExtensions =
            physical_device::enumerateDeviceExtensionProperties(physDevice);
        std::cout << "\n设备扩展 (" << deviceExtensions.size() << "个):\n";
        for (const auto &ext : deviceExtensions)
        {
            std::cout << "  - " << std::left << std::setw(60) << ext.extensionName
                      << " 版本: " << ext.specVersion << "\n";
        }

        // 4. 队列族信息
        auto queueFamilies = deviceWrapper.getQueueFamilyProperties();
        std::cout << "\n队列族 (" << queueFamilies.size() << "个):\n";
        for (size_t j = 0; j < queueFamilies.size(); ++j)
        {
            const auto &qf = queueFamilies[j];
            std::cout << "  队列族[" << j << "]: " << qf.queueCount << "个队列, 支持:";
            if (qf.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                std::cout << " 图形";
            if (qf.queueFlags & VK_QUEUE_COMPUTE_BIT)
                std::cout << " 计算";
            if (qf.queueFlags & VK_QUEUE_TRANSFER_BIT)
                std::cout << " 传输";
            if (qf.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
                std::cout << " 稀疏绑定";
            if (qf.queueFlags & VK_QUEUE_PROTECTED_BIT)
                std::cout << " 保护";
            std::cout << "\n";

            // 检查表面支持
            VkBool32 surfaceSupported =
                deviceWrapper.getSurfaceSupportKHR(static_cast<uint32_t>(j), surface);
            std::cout << "      表面支持: " << (surfaceSupported ? "是" : "否") << "\n";
        }

        // 5. 内存信息
        auto memoryProps = deviceWrapper.getMemoryProperties();
        std::cout << "\n内存堆 (" << memoryProps.memoryHeapCount << "个):\n";
        for (uint32_t j = 0; j < memoryProps.memoryHeapCount; ++j)
        {
            std::cout << "  堆[" << j
                      << "]: " << memoryProps.memoryHeaps[j].size / (1024 * 1024)
                      << " MB";
            if (memoryProps.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                std::cout << " (设备本地)";
            std::cout << "\n";
        }

        std::cout << "\n内存类型 (" << memoryProps.memoryTypeCount << "个):\n";
        for (uint32_t j = 0; j < memoryProps.memoryTypeCount; ++j)
        {
            std::cout << "  类型[" << j
                      << "]: 堆索引=" << memoryProps.memoryTypes[j].heapIndex;
            if (memoryProps.memoryTypes[j].propertyFlags &
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                std::cout << " 设备本地";
            if (memoryProps.memoryTypes[j].propertyFlags &
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                std::cout << " 主机可见";
            if (memoryProps.memoryTypes[j].propertyFlags &
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                std::cout << " 主机一致";
            if (memoryProps.memoryTypes[j].propertyFlags &
                VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                std::cout << " 主机缓存";
            std::cout << "\n";
        }

        // 6. 表面能力查询
        std::cout << "\n表面能力:\n";
        try
        {
            auto surfaceCapabilities = deviceWrapper.getSurfaceCapabilitiesKHR(surface);

            std::cout << "  最小图像数: " << surfaceCapabilities.minImageCount << "\n";
            std::cout << "  最大图像数: " << surfaceCapabilities.maxImageCount << "\n";
            std::cout << "  当前范围: " << surfaceCapabilities.currentExtent.width << "x"
                      << surfaceCapabilities.currentExtent.height << "\n";
            std::cout << "  最小范围: " << surfaceCapabilities.minImageExtent.width << "x"
                      << surfaceCapabilities.minImageExtent.height << "\n";
            std::cout << "  最大范围: " << surfaceCapabilities.maxImageExtent.width << "x"
                      << surfaceCapabilities.maxImageExtent.height << "\n";
            std::cout << "  最大图像数组层数: " << surfaceCapabilities.maxImageArrayLayers
                      << "\n";

            std::cout << "  支持的变换:";
            if (surfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
                std::cout << " 恒等";
            if (surfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR)
                std::cout << " 旋转90°";
            if (surfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR)
                std::cout << " 旋转180°";
            if (surfaceCapabilities.supportedTransforms &
                VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
                std::cout << " 旋转270°";
            std::cout << "\n";

            std::cout << "  支持的复合Alpha:";
            if (surfaceCapabilities.supportedCompositeAlpha &
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
                std::cout << " 不透明";
            if (surfaceCapabilities.supportedCompositeAlpha &
                VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
                std::cout << " 预乘";
            if (surfaceCapabilities.supportedCompositeAlpha &
                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
                std::cout << " 后乘";
            std::cout << "\n";

            std::cout << "  支持的用法标志:";
            if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                std::cout << " 传输源";
            if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                std::cout << " 传输目标";
            if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
                std::cout << " 采样";
            if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
                std::cout << " 存储";
            if (surfaceCapabilities.supportedUsageFlags &
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                std::cout << " 颜色附件";
            if (surfaceCapabilities.supportedUsageFlags &
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                std::cout << " 深度模板附件";
            std::cout << "\n";
        }
        catch (...)
        {
            std::cout << "  无法查询表面能力\n";
        }

        // 7. 表面格式
        std::cout << "\n支持的表面格式:\n";
        try
        {
            auto surfaceFormats = deviceWrapper.getSurfaceFormatsKHR(surface);
            for (const auto &format : surfaceFormats)
            {
                std::cout << "  - 格式: " << formatToString(format.format)
                          << ", 颜色空间: " << colorSpaceToString(format.colorSpace)
                          << "\n";
            }
        }
        catch (...)
        {
            std::cout << "  无法查询表面格式\n";
        }

        // 8. 呈现模式
        std::cout << "\n支持的呈现模式:\n";
        try
        {
            auto presentModes = deviceWrapper.getSurfacePresentModesKHR(surface);
            for (const auto &mode : presentModes)
            {
                std::cout << "  - " << presentModeToString(mode) << "\n";
            }
        }
        catch (...)
        {
            std::cout << "  无法查询呈现模式\n";
        }

        // 9. 格式属性查询
        std::cout << "\n格式属性:\n";
        VkFormat testFormats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM,
                                  VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_D32_SFLOAT,
                                  VK_FORMAT_D24_UNORM_S8_UINT};

        for (auto format : testFormats)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physDevice, format, &props);
            std::cout << "  " << formatToString(format) << ":\n";
            std::cout << "    线性平铺特性: 0x" << std::hex << props.linearTilingFeatures
                      << std::dec << "\n";
            std::cout << "    优化平铺特性: 0x" << std::hex << props.optimalTilingFeatures
                      << std::dec << "\n";
            std::cout << "    缓冲区特性: 0x" << std::hex << props.bufferFeatures
                      << std::dec << "\n";
        }

        // 10. 检查是否支持Vulkan 1.1+特性
        if (props.apiVersion >= VK_MAKE_VERSION(1, 1, 0))
        {
            std::cout << "\nVulkan 1.1+ 特性:\n";

            structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan11Features,
                            VkPhysicalDeviceVulkan12Features,
                            VkPhysicalDeviceVulkan13Features>
                featuresChain{{}, {}, {}, {}};

            deviceWrapper.getFeatures2(featuresChain.head());

            auto &vulkan11Features =
                featuresChain.get<VkPhysicalDeviceVulkan11Features>();
            auto &vulkan12Features =
                featuresChain.get<VkPhysicalDeviceVulkan12Features>();
            auto &vulkan13Features =
                featuresChain.get<VkPhysicalDeviceVulkan13Features>();

            std::cout << "  Vulkan 1.1:\n";
            std::cout << "    存储缓冲区16位访问: "
                      << (vulkan11Features.storageBuffer16BitAccess ? "是" : "否")
                      << "\n";
            std::cout << "    统一块中的变量: "
                      << (vulkan11Features.uniformAndStorageBuffer16BitAccess ? "是"
                                                                              : "否")
                      << "\n";

            std::cout << "  Vulkan 1.2:\n";
            std::cout << "    描述符索引: "
                      << (vulkan12Features.descriptorIndexing ? "是" : "否") << "\n";
            std::cout << "    时间线信号量: "
                      << (vulkan12Features.timelineSemaphore ? "是" : "否") << "\n";
            std::cout << "    缓冲区设备地址: "
                      << (vulkan12Features.bufferDeviceAddress ? "是" : "否") << "\n";

            std::cout << "  Vulkan 1.3:\n";
            std::cout << "    动态渲染: "
                      << (vulkan13Features.dynamicRendering ? "是" : "否") << "\n";
            std::cout << "    同步2: "
                      << (vulkan13Features.synchronization2 ? "是" : "否") << "\n";
            std::cout << "    主子通道: " << (vulkan13Features.maintenance4 ? "是" : "否")
                      << "\n";
        }

        // 11. 限制信息
        std::cout << "\n设备限制:\n";
        std::cout << "  最大图像尺寸1D: " << props.limits.maxImageDimension1D << "\n";
        std::cout << "  最大图像尺寸2D: " << props.limits.maxImageDimension2D << "\n";
        std::cout << "  最大图像尺寸3D: " << props.limits.maxImageDimension3D << "\n";
        std::cout << "  最大纹理尺寸: " << props.limits.maxTexelBufferElements << "\n";
        std::cout << "  最大统一缓冲区范围: " << props.limits.maxUniformBufferRange
                  << "\n";
        std::cout << "  最大存储缓冲区范围: " << props.limits.maxStorageBufferRange
                  << "\n";
        std::cout << "  最大描述符集数: " << props.limits.maxBoundDescriptorSets << "\n";
        std::cout << "  最大视口数: " << props.limits.maxViewports << "\n";
        std::cout << "  最大视口尺寸: " << props.limits.maxViewportDimensions[0] << "x"
                  << props.limits.maxViewportDimensions[1] << "\n";
        std::cout << "  视口边界范围: [" << props.limits.viewportBoundsRange[0] << ", "
                  << props.limits.viewportBoundsRange[1] << "]\n";
        std::cout << "  最大片段输出附件: " << props.limits.maxFragmentOutputAttachments
                  << "\n";
    }
}

void queryLogicalDeviceInfo(const VkPhysicalDevice &physDevice, VkSurfaceKHR surface)
{
    printSeparator("逻辑设备信息查询");

    // 尝试创建逻辑设备来查询更多信息
    std::vector<const char *> requiredExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // 查找支持图形和表面的队列族
    physical_device deviceWrapper(physDevice);
    auto queueFamilies = deviceWrapper.getQueueFamilyProperties();

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            VkBool32 surfaceSupported = deviceWrapper.getSurfaceSupportKHR(i, surface);
            if (surfaceSupported)
            {
                graphicsQueueFamilyIndex = i;
                break;
            }
        }
    }

    if (graphicsQueueFamilyIndex == UINT32_MAX)
    {
        std::cout << "未找到支持图形和表面的队列族，跳过逻辑设备创建\n";
        return;
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphicsQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority};

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.geometryShader = VK_TRUE;
    deviceFeatures.tessellationShader = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data(),
        .pEnabledFeatures = &deviceFeatures};

    try
    {
        VkDevice device;
        if (vkCreateDevice(physDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
        {
            std::cout << "创建逻辑设备失败\n";
            return;
        }

        std::cout << "逻辑设备创建成功\n";

        // 查询队列信息
        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
        std::cout << "图形队列句柄: " << graphicsQueue << "\n";

        // 查询设备内存信息
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);

        // 清理
        vkDestroyDevice(device, nullptr);
    }
    catch (...)
    {
        std::cout << "创建逻辑设备时发生异常\n";
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

        // 6. 枚举物理设备并查询逻辑设备信息
        uint32_t physicalDeviceCount = 0;
        vkEnumeratePhysicalDevices(instance.ref_data(), &physicalDeviceCount, nullptr);
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(instance.ref_data(), &physicalDeviceCount,
                                   physicalDevices.data());

        if (!physicalDevices.empty())
        {
            queryLogicalDeviceInfo(physicalDevices[0], surface);
        }

        // 7. 清理资源
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