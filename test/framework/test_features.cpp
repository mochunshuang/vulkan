#include "./head.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <expected>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <iomanip>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using surface = mcs::vulkan::wsi::glfw::Window;
using make_physical_device = mcs::vulkan::make_physical_device;
using make_instance = mcs::vulkan::make_instance;
using physical_device = mcs::vulkan::physical_device;
using make_logical_device = mcs::vulkan::make_logical_device;
using make_queue_family_index = mcs::vulkan::make_queue_family_index;
using logical_device = mcs::vulkan::logical_device;
using mcs::vulkan::structure_chain;
using mcs::vulkan::sType;

// NOTE: optional 或 expected 的and_then or_else 或许还能优化
struct vulkan_feature_check
{
    struct result_type
    {
        vulkan_feature_check *self;
        // 转换操作符，支持链式调用
        operator vulkan_feature_check *() noexcept // NOLINT
        {
            return self;
        }

        // 成员访问代理
        vulkan_feature_check *operator->() noexcept // NOLINT
        {
            return self;
        }
    };
    using check_result = std::expected<result_type, std::string>;
    using error_type = std::unexpected<std::string>;

    auto checkExtension(const char *extension_name) -> check_result
    {
        for (const auto &extension : availableExtensions_)
        {
            if (std::string_view{extension.extensionName} ==
                std::string_view{extension_name})
                return check_result{result_type{this}};
        }
        return error_type(
            std::format("extension: {} not supported [❌]", extension_name));
    }
    auto checkExtension(const std::vector<const char *> &extensions) -> check_result
    {
        for (const auto *chck_name : extensions)
            if (auto num = checkExtension(chck_name); not num.has_value())
                return num;
        return check_result{result_type{this}};
    }
    // structure_chain<
    template <typename... T>
    auto getDeviceFeatures(structure_chain<VkPhysicalDeviceFeatures2, T...> &features)
    {
        ::vkGetPhysicalDeviceFeatures2(physicalDevice_, &features.head());
        return features;
    }
    template <typename... T>
    auto getDeviceProperties(structure_chain<VkPhysicalDeviceProperties2, T...> &features)
    {
        ::vkGetPhysicalDeviceProperties2(physicalDevice_, &features.head());
        return features;
    }
    template <typename... T>
    auto checkDeviceFeatures(auto fn) -> check_result
        requires(requires(structure_chain<VkPhysicalDeviceFeatures2, T...> result) {
            fn(result);
        })
    {
        auto features = structure_chain<VkPhysicalDeviceFeatures2, T...>{{}, T{}...};
        if (fn(getDeviceFeatures(features)))
            return check_result{result_type{this}};
        return error_type(std::format("one of deviceFeatures not supported [❌]"));
    }
    template <typename... T>
    auto checkDeviceProperties(auto fn) -> check_result
        requires(requires(structure_chain<VkPhysicalDeviceProperties2, T...> result) {
            fn(result);
        })
    {
        auto features = structure_chain<VkPhysicalDeviceProperties2, T...>{{}, T{}...};
        if (fn(getDeviceProperties(features)))
            return check_result{result_type{this}};
        return std::unexpected(std::format("one of deviceProperties not supported [❌]"));
    }

    static void printBase(VkPhysicalDevice physicalDevice)
    {
        // 1. 获取完整的设备属性结构
        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        std::cout << "\n=============== 物理设备完整属性 (C API) ===============\n";

        // 2. 基础信息
        std::cout << "=== 1. 基础信息 ===\n";
        std::cout << "  设备名称: " << props.deviceName << '\n';
        std::cout << "  设备类型: ";
        switch (props.deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            std::cout << "集成GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            std::cout << "独立GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            std::cout << "虚拟GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            std::cout << "CPU";
            break;
        default:
            std::cout << "其他";
        }
        std::cout << '\n';
        std::cout << "  API 版本: " << VK_VERSION_MAJOR(props.apiVersion) << "."
                  << VK_VERSION_MINOR(props.apiVersion) << "."
                  << VK_VERSION_PATCH(props.apiVersion) << '\n';
        std::cout << "  驱动版本: 0x" << std::hex << props.driverVersion << std::dec
                  << '\n';

        // 3. 厂商和设备ID
        std::cout << "\n=== 2. 识别信息 ===\n";
        std::cout << "  厂商ID (Vendor ID): 0x" << std::hex << props.vendorID << std::dec;
        // 可选：添加常见厂商ID的识别
        switch (props.vendorID)
        {
        case 0x10DE:
            std::cout << " (NVIDIA)";
            break;
        case 0x1002:
            std::cout << " (AMD)";
            break;
        case 0x8086:
            std::cout << " (Intel)";
            break;
        case 0x13B5:
            std::cout << " (ARM)";
            break;
        }
        std::cout << '\n';
        std::cout << "  设备ID (Device ID): 0x" << std::hex << props.deviceID << std::dec
                  << '\n';

        // 4. 管道缓存UUID (用于识别缓存兼容性)
        std::cout << "  管道缓存UUID: ";
        for (uint32_t i = 0; i < VK_UUID_SIZE; ++i)
        {
            if (i > 0)
                std::cout << "-";
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(props.pipelineCacheUUID[i]);
        }
        std::cout << std::dec << '\n';

        // 5. 设备限制 (精选最重要的部分)
        const VkPhysicalDeviceLimits &limits = props.limits;
        std::cout << "\n=== 3. 设备限制 (关键项目) ===\n";

        // 5.1 图像相关限制
        std::cout << "  [图像限制]\n";
        std::cout << "    最大 1D 图像尺寸: " << limits.maxImageDimension1D << '\n';
        std::cout << "    最大 2D 图像尺寸: " << limits.maxImageDimension2D << '\n';
        std::cout << "    最大 3D 图像尺寸: " << limits.maxImageDimension3D << '\n';
        std::cout << "    最大立方体贴图尺寸: " << limits.maxImageDimensionCube << '\n';
        std::cout << "    最大图像数组层数: " << limits.maxImageArrayLayers << '\n';
        std::cout << "    最大帧缓冲颜色附件数: " << limits.maxColorAttachments << '\n';

        // 5.2 视口和剪裁
        std::cout << "  [视口与剪裁]\n";
        std::cout << "    最大视口数量: " << limits.maxViewports << '\n';
        std::cout << "    最大视口尺寸: [" << limits.maxViewportDimensions[0] << ", "
                  << limits.maxViewportDimensions[1] << "]\n";
        std::cout << "    视口边界范围: [" << limits.viewportBoundsRange[0] << ", "
                  << limits.viewportBoundsRange[1] << "]\n";

        // 5.3 缓冲区相关限制
        std::cout << "  [缓冲区限制]\n";
        std::cout << "    最大统一缓冲区范围: " << limits.maxUniformBufferRange
                  << " 字节\n";
        std::cout << "    最大存储缓冲区范围: " << limits.maxStorageBufferRange
                  << " 字节\n";
        std::cout << "    最大 texel 缓冲区元素: " << limits.maxTexelBufferElements
                  << '\n';

        // 5.4 描述符相关限制
        std::cout << "  [描述符限制]\n";
        std::cout << "    最大绑定描述符集数: " << limits.maxBoundDescriptorSets << '\n';
        std::cout << "    最大每阶段采样器数: " << limits.maxPerStageDescriptorSamplers
                  << '\n';
        std::cout << "    最大每阶段统一缓冲区数: "
                  << limits.maxPerStageDescriptorUniformBuffers << '\n';
        std::cout << "    最大每阶段存储缓冲区数: "
                  << limits.maxPerStageDescriptorStorageBuffers << '\n';
        std::cout << "    最大每阶段采样图像数: "
                  << limits.maxPerStageDescriptorSampledImages << '\n';

        // 5.5 着色器相关限制
        std::cout << "  [着色器限制]\n";
        std::cout << "    最大顶点属性数: " << limits.maxVertexInputAttributes << '\n';
        std::cout << "    最大顶点绑定数: " << limits.maxVertexInputBindings << '\n';
        std::cout << "    最大顶点属性偏移: " << limits.maxVertexInputAttributeOffset
                  << '\n';
        std::cout << "    最大顶点绑定 stride: " << limits.maxVertexInputBindingStride
                  << '\n';
        std::cout << "    最大 clip distance: " << limits.maxClipDistances << '\n';
        std::cout << "    最大 cull distance: " << limits.maxCullDistances << '\n';
        std::cout << "    最大组合 clip/cull distance: "
                  << limits.maxCombinedClipAndCullDistances << '\n';

        // 5.6 计算着色器限制
        std::cout << "  [计算着色器]\n";
        std::cout << "    最大计算工作组数量: [" << limits.maxComputeWorkGroupCount[0]
                  << ", " << limits.maxComputeWorkGroupCount[1] << ", "
                  << limits.maxComputeWorkGroupCount[2] << "]\n";
        std::cout << "    最大计算工作组大小: [" << limits.maxComputeWorkGroupSize[0]
                  << ", " << limits.maxComputeWorkGroupSize[1] << ", "
                  << limits.maxComputeWorkGroupSize[2] << "]\n";
        std::cout << "    最大计算工作组调用数: " << limits.maxComputeWorkGroupInvocations
                  << '\n';

        // 5.7 其他重要限制
        std::cout << "  [其他限制]\n";
        std::cout << "    子像素精度位数: " << limits.subPixelPrecisionBits << '\n';
        std::cout << "    子 texel 精度位数: " << limits.subTexelPrecisionBits << '\n';
        std::cout << "    mipmap 精度位数: " << limits.mipmapPrecisionBits << '\n';
        std::cout << "    点大小范围: [" << limits.pointSizeRange[0] << ", "
                  << limits.pointSizeRange[1] << "]\n";
        std::cout << "    线宽范围: [" << limits.lineWidthRange[0] << ", "
                  << limits.lineWidthRange[1] << "]\n";

        // 6. 稀疏内存属性
        const VkPhysicalDeviceSparseProperties &sparse = props.sparseProperties;
        std::cout << "\n=== 4. 稀疏内存属性 ===\n";
        std::cout << "  支持资源内存绑定: "
                  << (sparse.residencyStandard2DBlockShape ? "是" : "否") << '\n';
        std::cout << "  支持 2D 图像标准块形状: "
                  << (sparse.residencyStandard2DBlockShape ? "是" : "否") << '\n';
        std::cout << "  支持 3D 图像标准块形状: "
                  << (sparse.residencyStandard3DBlockShape ? "是" : "否") << '\n';
        std::cout << "  支持非 2:1 对齐的 mipmap: "
                  << (sparse.residencyNonResidentStrict ? "是" : "否") << '\n';
        std::cout << "  非驻留严格匹配: "
                  << (sparse.residencyNonResidentStrict ? "是" : "否") << '\n';
        std::cout << "  支持稀疏图像数组: "
                  << (sparse.residencyAlignedMipSize ? "是" : "否") << '\n';

        std::cout << "========================================================\n";
    }
    explicit vulkan_feature_check(VkPhysicalDevice physicalDevice)
        : physicalDevice_{physicalDevice}
    {
        assert(physicalDevice != nullptr);
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount,
                                             nullptr);
        availableExtensions_.resize(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount,
                                             availableExtensions_.data());

        printBase(physicalDevice);
    }

  private:
    VkPhysicalDevice physicalDevice_;
    std::vector<VkExtensionProperties> availableExtensions_;
};

template <typename T>
consteval uint32_t VkApiVersion(T const variant, T const major, T const minor,
                                T const patch = 0)
    requires std::is_integral<T>::value
{
    return ((((uint32_t)(variant)) << 29U) | (((uint32_t)(major)) << 22U) |
            (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)));
}

// template <typename T>
// concept has_stype = requires {
//     { sType<T>() } noexcept -> std::same_as<VkStructureType>;
// };
template <typename T>
concept has_stype = std::is_same_v<decltype(sType<T>()), VkStructureType>;

int main()
{

    // 1. 创建Vulkan实例
    auto instance = make_instance{}
                        .enableDebugExtension()
                        .enableSurfaceExtension<surface>()
                        .checkExtensionSupport()
                        .checkLayerSupport()
                        .build({.sType = sType<VkApplicationInfo>(),
                                .pApplicationName = "Single Pipeline Demo",
                                .applicationVersion = VkApiVersion(1, 0, 0),
                                .pEngineName = "No Engine",
                                .engineVersion = VkApiVersion(1, 0, 0),
                                .apiVersion = VkApiVersion(0, 1, 3, 0)});
    // 2. 选择物理设备
    std::vector<const char *> requiredDeviceExtension = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME};

    structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan13Features,
                    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
        enablefeatureChain = {{},
                              {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                              {.extendedDynamicState = VK_TRUE}};
    auto physicalDevice =
        make_physical_device{instance.ref_data()}
            .requiredDeviceProperties([](const VkPhysicalDeviceProperties
                                             &device_properties) constexpr noexcept {
                return device_properties.apiVersion >= VK_API_VERSION_1_3;
            })
            .requiredQueueFamilyProperties(
                [](const VkQueueFamilyProperties &qfp) constexpr noexcept {
                    return !!(qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT);
                })
            .requiredDeviceExtensions(requiredDeviceExtension)
            .requiredFeatures([](const physical_device &physicalDevice) constexpr noexcept
                                  -> bool {
                auto query =
                    structure_chain<VkPhysicalDeviceFeatures2,
                                    VkPhysicalDeviceVulkan13Features,
                                    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>{
                        {}, {}, {}};
                physicalDevice.getFeatures2(query.head());
                auto &query_features = query.head().features;
                auto &query_vulkan13_features =
                    query.template get<VkPhysicalDeviceVulkan13Features>();
                auto &query_extended_dynamic_state_features =
                    query.template get<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();

                return query_features.fillModeNonSolid && query_features.wideLines &&
                       query_vulkan13_features.dynamicRendering &&
                       query_vulkan13_features.synchronization2 &&
                       query_extended_dynamic_state_features.extendedDynamicState;
            })
            .pickPhysicalDevice();

    // 使用 vulkan_feature_check 检查特性
    vulkan_feature_check feature_check{physicalDevice.raw_data()};
    using check_result = vulkan_feature_check::check_result;
    using error_type = vulkan_feature_check::error_type;

    [[maybe_unused]] auto _ =
        feature_check
            .checkExtension(std::vector<const char *>{"VK_EXT_extended_dynamic_state3"})
            .and_then([](check_result ret) -> check_result {
                std::cout << "VK_EXT_extended_dynamic_state3 支持✅";
                return ret.value()
                    ->checkDeviceFeatures<
                        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT>(
                        [](const structure_chain<
                            VkPhysicalDeviceFeatures2,
                            VkPhysicalDeviceExtendedDynamicState3FeaturesEXT> &result)
                            -> bool {
                            const VkPhysicalDeviceFeatures2 &features2 = result.get<0>();
                            const VkPhysicalDeviceExtendedDynamicState3FeaturesEXT
                                &features1 = result.get<1>();

                            // 检查一些核心特性
                            std::cout
                                << "| samplerAnisotropy (各向异性过滤) | "
                                << (features2.features.samplerAnisotropy ? "✅ 支持"
                                                                         : "❌ 不支持")
                                << " | ✅ 支持          |" << '\n';

                            std::cout << "| geometryShader (几何着色器)      | "
                                      << (features2.features.geometryShader ? "✅ 支持"
                                                                            : "❌ 不支持")
                                      << " | ✅ 支持          |" << '\n';

                            std::cout
                                << "| tessellationShader (细分着色器)  | "
                                << (features2.features.tessellationShader ? "✅ 支持"
                                                                          : "❌ 不支持")
                                << " | ✅ 支持          |" << '\n';

                            return true;
                        })
                    .value()
                    ->checkDeviceProperties<
                        VkPhysicalDeviceExtendedDynamicState3PropertiesEXT>(
                        [](const structure_chain<
                            VkPhysicalDeviceProperties2,
                            VkPhysicalDeviceExtendedDynamicState3PropertiesEXT> &result) {
                            const VkPhysicalDeviceProperties2 &features0 =
                                result.get<0>();
                            const VkPhysicalDeviceExtendedDynamicState3PropertiesEXT
                                &extProps3 = result.get<1>();
                            std::cout << "| dynamicPrimitiveTopologyUnrestricted | "
                                      << (extProps3.dynamicPrimitiveTopologyUnrestricted
                                              ? "✅ 支持"
                                              : "❌ 不支持")
                                      << '\n';
                            return true;
                        });
            });

    // 在main函数中添加bindless特性检查
    {
        std::cout << "\n=== 检查描述符索引(bindless)特性 ===" << '\n';

        // 检查Vulkan 1.2特性（描述符索引在Vulkan 1.2中成为核心特性）
        auto result =
            feature_check
                .checkDeviceProperties<VkPhysicalDeviceVulkan12Properties>(
                    [](const structure_chain<VkPhysicalDeviceProperties2,
                                             VkPhysicalDeviceVulkan12Properties> &props)
                        -> bool {
                        const auto &vulkan12_props = props.get<1>();
                        std::cout
                            << "最大更新后绑定数: "
                            << vulkan12_props.maxDescriptorSetUpdateAfterBindSamplers
                            << '\n';
                        return true;
                    })
                .and_then([](check_result ret) {
                    return ret.value()
                        ->checkDeviceFeatures<VkPhysicalDeviceVulkan12Features>(
                            [](const structure_chain<VkPhysicalDeviceFeatures2,
                                                     VkPhysicalDeviceVulkan12Features>
                                   &features) -> bool {
                                const auto &vulkan12_features = features.get<1>();

                                std::cout << "Vulkan 1.2 描述符索引特性:" << '\n';
                                std::cout << "  - descriptorIndexing: "
                                          << (vulkan12_features.descriptorIndexing ? "✅"
                                                                                   : "❌")
                                          << '\n';
                                std::cout
                                    << "  - runtimeDescriptorArray: "
                                    << (vulkan12_features.runtimeDescriptorArray ? "✅"
                                                                                 : "❌")
                                    << '\n';

                                return vulkan12_features.descriptorIndexing;
                            });
                })
                .and_then([](check_result ret) {
                    // 如果需要额外检查EXT扩展
                    return ret.value()->checkExtension("VK_EXT_descriptor_indexing");
                });

        if (!result.has_value())
        {
            std::cout << "警告: 描述符索引功能不完全支持" << '\n';
        }
        else
        {
            std::cout << "✅ 描述符索引(bindless)支持确认" << '\n';
        }
    }

    static_assert(has_stype<VkPhysicalDeviceVulkan12Features>);
    static_assert(has_stype<int>); // NOTE: clang BUG。

    return 0;
}