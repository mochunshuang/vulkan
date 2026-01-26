#pragma once

#include <format>
#include <vulkan/vulkan.h>
#include <string>

namespace mcs::vulkan
{
    static constexpr std::string to_hex_string(uint32_t value)
    {
        return std::format("{:x}", value);
    }

    // VkResult
    static constexpr std::string to_string(VkResult value)
    {
        switch (value)
        {
        case VK_SUCCESS:
            return "Success";
        case VK_NOT_READY:
            return "NotReady";
        case VK_TIMEOUT:
            return "Timeout";
        case VK_EVENT_SET:
            return "EventSet";
        case VK_EVENT_RESET:
            return "EventReset";
        case VK_INCOMPLETE:
            return "Incomplete";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "ErrorOutOfHostMemory";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "ErrorOutOfDeviceMemory";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "ErrorInitializationFailed";
        case VK_ERROR_DEVICE_LOST:
            return "ErrorDeviceLost";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "ErrorMemoryMapFailed";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "ErrorLayerNotPresent";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "ErrorExtensionNotPresent";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "ErrorFeatureNotPresent";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "ErrorIncompatibleDriver";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "ErrorTooManyObjects";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "ErrorFormatNotSupported";
        case VK_ERROR_FRAGMENTED_POOL:
            return "ErrorFragmentedPool";
        case VK_ERROR_UNKNOWN:
            return "ErrorUnknown";
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return "ErrorOutOfPoolMemory";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return "ErrorInvalidExternalHandle";
        case VK_ERROR_FRAGMENTATION:
            return "ErrorFragmentation";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            return "ErrorInvalidOpaqueCaptureAddress";
        case VK_PIPELINE_COMPILE_REQUIRED:
            return "PipelineCompileRequired";
        case VK_ERROR_NOT_PERMITTED:
            return "ErrorNotPermitted";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "ErrorSurfaceLostKHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "ErrorNativeWindowInUseKHR";
        case VK_SUBOPTIMAL_KHR:
            return "SuboptimalKHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "ErrorOutOfDateKHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "ErrorIncompatibleDisplayKHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "ErrorValidationFailedEXT";
        case VK_ERROR_INVALID_SHADER_NV:
            return "ErrorInvalidShaderNV";
        case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
            return "ErrorImageUsageNotSupportedKHR";
        case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
            return "ErrorVideoPictureLayoutNotSupportedKHR";
        case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
            return "ErrorVideoProfileOperationNotSupportedKHR";
        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
            return "ErrorVideoProfileFormatNotSupportedKHR";
        case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
            return "ErrorVideoProfileCodecNotSupportedKHR";
        case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
            return "ErrorVideoStdVersionNotSupportedKHR";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
            return "ErrorInvalidDrmFormatModifierPlaneLayoutEXT";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
            return "ErrorFullScreenExclusiveModeLostEXT";
        case VK_THREAD_IDLE_KHR:
            return "ThreadIdleKHR";
        case VK_THREAD_DONE_KHR:
            return "ThreadDoneKHR";
        case VK_OPERATION_DEFERRED_KHR:
            return "OperationDeferredKHR";
        case VK_OPERATION_NOT_DEFERRED_KHR:
            return "OperationNotDeferredKHR";
        case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
            return "ErrorInvalidVideoStdParametersKHR";
        case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
            return "ErrorCompressionExhaustedEXT";
        case VK_INCOMPATIBLE_SHADER_BINARY_EXT:
            return "IncompatibleShaderBinaryEXT";
        case VK_PIPELINE_BINARY_MISSING_KHR:
            return "PipelineBinaryMissingKHR";
        case VK_ERROR_NOT_ENOUGH_SPACE_KHR:
            return "ErrorNotEnoughSpaceKHR";
        default:
            return "invalid ( " + to_hex_string(static_cast<uint32_t>(value)) + " )";
        }
    }

    static constexpr std::string to_string(VkPhysicalDeviceType value)
    {
        switch (value)
        {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return "Other";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "IntegratedGpu";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "DiscreteGpu";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "VirtualGpu";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "Cpu";
        case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:
            return "MaxEnum";
        default:
            return "invalid ( " + to_hex_string(static_cast<uint32_t>(value)) + " )";
        }
    }
    // NOLINTBEGIN
    constexpr static std::string to_string(VkSampleCountFlags flags)
    {
        std::string result;
        if (flags & VK_SAMPLE_COUNT_1_BIT)
            result += "1|";
        if (flags & VK_SAMPLE_COUNT_2_BIT)
            result += "2|";
        if (flags & VK_SAMPLE_COUNT_4_BIT)
            result += "4|";
        if (flags & VK_SAMPLE_COUNT_8_BIT)
            result += "8|";
        if (flags & VK_SAMPLE_COUNT_16_BIT)
            result += "16|";
        if (flags & VK_SAMPLE_COUNT_32_BIT)
            result += "32|";
        if (flags & VK_SAMPLE_COUNT_64_BIT)
            result += "64|";

        if (!result.empty())
            result.pop_back(); // Remove trailing '|'
        return result.empty() ? "None" : result;
    } // NOLINTEND

    constexpr static std::string to_string(const VkPhysicalDeviceLimits &limits)
    {
        return std::format(
            "PhysicalDeviceLimits {{\n"
            "  maxImageDimension1D: {},\n"
            "  maxImageDimension2D: {},\n"
            "  maxImageDimension3D: {},\n"
            "  maxImageDimensionCube: {},\n"
            "  maxImageArrayLayers: {},\n"
            "  maxTexelBufferElements: {},\n"
            "  maxUniformBufferRange: {},\n"
            "  maxStorageBufferRange: {},\n"
            "  maxPushConstantsSize: {},\n"
            "  maxMemoryAllocationCount: {},\n"
            "  maxSamplerAllocationCount: {},\n"
            "  bufferImageGranularity: {},\n"
            "  sparseAddressSpaceSize: {},\n"
            "  maxBoundDescriptorSets: {},\n"
            "  maxPerStageDescriptorSamplers: {},\n"
            "  maxPerStageDescriptorUniformBuffers: {},\n"
            "  maxPerStageDescriptorStorageBuffers: {},\n"
            "  maxPerStageDescriptorSampledImages: {},\n"
            "  maxPerStageDescriptorStorageImages: {},\n"
            "  maxPerStageDescriptorInputAttachments: {},\n"
            "  maxPerStageResources: {},\n"
            "  maxDescriptorSetSamplers: {},\n"
            "  maxDescriptorSetUniformBuffers: {},\n"
            "  maxDescriptorSetUniformBuffersDynamic: {},\n"
            "  maxDescriptorSetStorageBuffers: {},\n"
            "  maxDescriptorSetStorageBuffersDynamic: {},\n"
            "  maxDescriptorSetSampledImages: {},\n"
            "  maxDescriptorSetStorageImages: {},\n"
            "  maxDescriptorSetInputAttachments: {},\n"
            "  maxVertexInputAttributes: {},\n"
            "  maxVertexInputBindings: {},\n"
            "  maxVertexInputAttributeOffset: {},\n"
            "  maxVertexInputBindingStride: {},\n"
            "  maxVertexOutputComponents: {},\n"
            "  maxTessellationGenerationLevel: {},\n"
            "  maxTessellationPatchSize: {},\n"
            "  maxTessellationControlPerVertexInputComponents: {},\n"
            "  maxTessellationControlPerVertexOutputComponents: {},\n"
            "  maxTessellationControlPerPatchOutputComponents: {},\n"
            "  maxTessellationControlTotalOutputComponents: {},\n"
            "  maxTessellationEvaluationInputComponents: {},\n"
            "  maxTessellationEvaluationOutputComponents: {},\n"
            "  maxGeometryShaderInvocations: {},\n"
            "  maxGeometryInputComponents: {},\n"
            "  maxGeometryOutputComponents: {},\n"
            "  maxGeometryOutputVertices: {},\n"
            "  maxGeometryTotalOutputComponents: {},\n"
            "  maxFragmentInputComponents: {},\n"
            "  maxFragmentOutputAttachments: {},\n"
            "  maxFragmentDualSrcAttachments: {},\n"
            "  maxFragmentCombinedOutputResources: {},\n"
            "  maxComputeSharedMemorySize: {},\n"
            "  maxComputeWorkGroupCount: [{}, {}, {}],\n"
            "  maxComputeWorkGroupInvocations: {},\n"
            "  maxComputeWorkGroupSize: [{}, {}, {}],\n"
            "  subPixelPrecisionBits: {},\n"
            "  subTexelPrecisionBits: {},\n"
            "  mipmapPrecisionBits: {},\n"
            "  maxDrawIndexedIndexValue: {},\n"
            "  maxDrawIndirectCount: {},\n"
            "  maxSamplerLodBias: {},\n"
            "  maxSamplerAnisotropy: {},\n"
            "  maxViewports: {},\n"
            "  maxViewportDimensions: [{}, {}],\n"
            "  viewportBoundsRange: [{}, {}],\n"
            "  viewportSubPixelBits: {},\n"
            "  minMemoryMapAlignment: {},\n"
            "  minTexelBufferOffsetAlignment: {},\n"
            "  minUniformBufferOffsetAlignment: {},\n"
            "  minStorageBufferOffsetAlignment: {},\n"
            "  minTexelOffset: {},\n"
            "  maxTexelOffset: {},\n"
            "  minTexelGatherOffset: {},\n"
            "  maxTexelGatherOffset: {},\n"
            "  minInterpolationOffset: {},\n"
            "  maxInterpolationOffset: {},\n"
            "  subPixelInterpolationOffsetBits: {},\n"
            "  maxFramebufferWidth: {},\n"
            "  maxFramebufferHeight: {},\n"
            "  maxFramebufferLayers: {},\n"
            "  framebufferColorSampleCounts: [{}],\n"
            "  framebufferDepthSampleCounts: [{}],\n"
            "  framebufferStencilSampleCounts: [{}],\n"
            "  framebufferNoAttachmentsSampleCounts: [{}],\n"
            "  maxColorAttachments: {},\n"
            "  sampledImageColorSampleCounts: [{}],\n"
            "  sampledImageIntegerSampleCounts: [{}],\n"
            "  sampledImageDepthSampleCounts: [{}],\n"
            "  sampledImageStencilSampleCounts: [{}],\n"
            "  storageImageSampleCounts: [{}],\n"
            "  maxSampleMaskWords: {},\n"
            "  timestampComputeAndGraphics: {},\n"
            "  timestampPeriod: {},\n"
            "  maxClipDistances: {},\n"
            "  maxCullDistances: {},\n"
            "  maxCombinedClipAndCullDistances: {},\n"
            "  discreteQueuePriorities: {},\n"
            "  pointSizeRange: [{}, {}],\n"
            "  lineWidthRange: [{}, {}],\n"
            "  pointSizeGranularity: {},\n"
            "  lineWidthGranularity: {},\n"
            "  strictLines: {},\n"
            "  standardSampleLocations: {},\n"
            "  optimalBufferCopyOffsetAlignment: {},\n"
            "  optimalBufferCopyRowPitchAlignment: {},\n"
            "  nonCoherentAtomSize: {}\n"
            "}}",
            limits.maxImageDimension1D, limits.maxImageDimension2D,
            limits.maxImageDimension3D, limits.maxImageDimensionCube,
            limits.maxImageArrayLayers, limits.maxTexelBufferElements,
            limits.maxUniformBufferRange, limits.maxStorageBufferRange,
            limits.maxPushConstantsSize, limits.maxMemoryAllocationCount,
            limits.maxSamplerAllocationCount, limits.bufferImageGranularity,
            limits.sparseAddressSpaceSize, limits.maxBoundDescriptorSets,
            limits.maxPerStageDescriptorSamplers,
            limits.maxPerStageDescriptorUniformBuffers,
            limits.maxPerStageDescriptorStorageBuffers,
            limits.maxPerStageDescriptorSampledImages,
            limits.maxPerStageDescriptorStorageImages,
            limits.maxPerStageDescriptorInputAttachments, limits.maxPerStageResources,
            limits.maxDescriptorSetSamplers, limits.maxDescriptorSetUniformBuffers,
            limits.maxDescriptorSetUniformBuffersDynamic,
            limits.maxDescriptorSetStorageBuffers,
            limits.maxDescriptorSetStorageBuffersDynamic,
            limits.maxDescriptorSetSampledImages, limits.maxDescriptorSetStorageImages,
            limits.maxDescriptorSetInputAttachments, limits.maxVertexInputAttributes,
            limits.maxVertexInputBindings, limits.maxVertexInputAttributeOffset,
            limits.maxVertexInputBindingStride, limits.maxVertexOutputComponents,
            limits.maxTessellationGenerationLevel, limits.maxTessellationPatchSize,
            limits.maxTessellationControlPerVertexInputComponents,
            limits.maxTessellationControlPerVertexOutputComponents,
            limits.maxTessellationControlPerPatchOutputComponents,
            limits.maxTessellationControlTotalOutputComponents,
            limits.maxTessellationEvaluationInputComponents,
            limits.maxTessellationEvaluationOutputComponents,
            limits.maxGeometryShaderInvocations, limits.maxGeometryInputComponents,
            limits.maxGeometryOutputComponents, limits.maxGeometryOutputVertices,
            limits.maxGeometryTotalOutputComponents, limits.maxFragmentInputComponents,
            limits.maxFragmentOutputAttachments, limits.maxFragmentDualSrcAttachments,
            limits.maxFragmentCombinedOutputResources, limits.maxComputeSharedMemorySize,
            limits.maxComputeWorkGroupCount[0], limits.maxComputeWorkGroupCount[1],
            limits.maxComputeWorkGroupCount[2], limits.maxComputeWorkGroupInvocations,
            limits.maxComputeWorkGroupSize[0], limits.maxComputeWorkGroupSize[1],
            limits.maxComputeWorkGroupSize[2], limits.subPixelPrecisionBits,
            limits.subTexelPrecisionBits, limits.mipmapPrecisionBits,
            limits.maxDrawIndexedIndexValue, limits.maxDrawIndirectCount,
            limits.maxSamplerLodBias, limits.maxSamplerAnisotropy, limits.maxViewports,
            limits.maxViewportDimensions[0], limits.maxViewportDimensions[1],
            limits.viewportBoundsRange[0], limits.viewportBoundsRange[1],
            limits.viewportSubPixelBits, limits.minMemoryMapAlignment,
            limits.minTexelBufferOffsetAlignment, limits.minUniformBufferOffsetAlignment,
            limits.minStorageBufferOffsetAlignment, limits.minTexelOffset,
            limits.maxTexelOffset, limits.minTexelGatherOffset,
            limits.maxTexelGatherOffset, limits.minInterpolationOffset,
            limits.maxInterpolationOffset, limits.subPixelInterpolationOffsetBits,
            limits.maxFramebufferWidth, limits.maxFramebufferHeight,
            limits.maxFramebufferLayers, to_string(limits.framebufferColorSampleCounts),
            to_string(limits.framebufferDepthSampleCounts),
            to_string(limits.framebufferStencilSampleCounts),
            to_string(limits.framebufferNoAttachmentsSampleCounts),
            limits.maxColorAttachments, to_string(limits.sampledImageColorSampleCounts),
            to_string(limits.sampledImageIntegerSampleCounts),
            to_string(limits.sampledImageDepthSampleCounts),
            to_string(limits.sampledImageStencilSampleCounts),
            to_string(limits.storageImageSampleCounts), limits.maxSampleMaskWords,
            limits.timestampComputeAndGraphics, limits.timestampPeriod,
            limits.maxClipDistances, limits.maxCullDistances,
            limits.maxCombinedClipAndCullDistances, limits.discreteQueuePriorities,
            limits.pointSizeRange[0], limits.pointSizeRange[1], limits.lineWidthRange[0],
            limits.lineWidthRange[1], limits.pointSizeGranularity,
            limits.lineWidthGranularity, limits.strictLines,
            limits.standardSampleLocations, limits.optimalBufferCopyOffsetAlignment,
            limits.optimalBufferCopyRowPitchAlignment, limits.nonCoherentAtomSize);
    }

    constexpr static std::string to_string(
        const VkPhysicalDeviceSparseProperties &sparseProps)
    {
        return std::format("PhysicalDeviceSparseProperties {{\n"
                           "  residencyStandard2DBlockShape: {},\n"
                           "  residencyStandard2DMultisampleBlockShape: {},\n"
                           "  residencyStandard3DBlockShape: {},\n"
                           "  residencyAlignedMipSize: {},\n"
                           "  residencyNonResidentStrict: {}\n"
                           "}}",
                           sparseProps.residencyStandard2DBlockShape,
                           sparseProps.residencyStandard2DMultisampleBlockShape,
                           sparseProps.residencyStandard3DBlockShape,
                           sparseProps.residencyAlignedMipSize,
                           sparseProps.residencyNonResidentStrict);
    }

    constexpr static std::string to_string(const VkPhysicalDeviceProperties &props)
    {
        std::string deviceName = props.deviceName;

        return std::format(
            "PhysicalDeviceProperties {{\n"
            "  apiVersion: {}.{}.{},\n"
            "  driverVersion: {}.{}.{},\n"
            "  vendorID: 0x{:04X},\n"
            "  deviceID: 0x{:04X},\n"
            "  deviceType: {},\n"
            "  deviceName: {},\n"
            "  pipelineCacheUUID: {},\n"
            "\n  Limits:\n{}\n"
            "\n  SparseProperties:\n{}\n"
            "}}",
            VK_API_VERSION_MAJOR(props.apiVersion),
            VK_API_VERSION_MINOR(props.apiVersion),
            VK_API_VERSION_PATCH(props.apiVersion), VK_VERSION_MAJOR(props.driverVersion),
            VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion),
            props.vendorID, props.deviceID, to_string(props.deviceType), deviceName,
            std::to_string(props.pipelineCacheUUID[0]), // 简化处理UUID
            to_string(props.limits), to_string(props.sparseProperties));
    }

    // NOLINTBEGIN
    static constexpr std::string to_string(VkQueueFlagBits value)
    {
        std::string result = "{";
        if (value & VK_QUEUE_GRAPHICS_BIT)
            result += " Graphics |";
        if (value & VK_QUEUE_COMPUTE_BIT)
            result += " Compute |";
        if (value & VK_QUEUE_TRANSFER_BIT)
            result += " Transfer |";
        if (value & VK_QUEUE_SPARSE_BINDING_BIT)
            result += " SparseBinding |";
        if (value & VK_QUEUE_PROTECTED_BIT)
            result += " Protected |";
        if (value & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
            result += " VideoDecodeKHR |";
        if (value & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
            result += " VideoEncodeKHR |";
        if (value & VK_QUEUE_OPTICAL_FLOW_BIT_NV)
            result += " OpticalFlowNV |";
        if (result.size() > 1)
        {
            result.pop_back(); // 移除最后一个空格
            result.back() = '}';
        }
        else
            result = "{}";
        return result;
    }

    // VkExtent3D
    static constexpr std::string to_string(const VkExtent3D &extent)
    {
        return std::format("[{}, {}, {}]", extent.width, extent.height, extent.depth);
    }

    // VkMemoryHeapFlags
    static constexpr std::string to_string(VkMemoryHeapFlagBits flags)
    {
        std::string result = "{";
        if (flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            result += " DeviceLocal |";
        if (flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT)
            result += " MultiInstance |";
        if (flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT_KHR)
            result += " MultiInstanceKHR |";

        if (result.size() > 1)
        {
            result.pop_back();
            result.back() = '}';
        }
        else
        {
            result = "{}";
        }
        return result;
    }
    // VkMemoryPropertyFlags
    static constexpr std::string to_string(VkMemoryPropertyFlagBits flags)
    {
        std::string result = "{";
        if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            result += " DeviceLocal |";
        if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            result += " HostVisible |";
        if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            result += " HostCoherent |";
        if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
            result += " HostCached |";
        if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
            result += " LazilyAllocated |";
        if (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
            result += " Protected |";
        if (flags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)
            result += " DeviceCoherentAMD |";
        if (flags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)
            result += " DeviceUncachedAMD |";
        if (flags & VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV)
            result += " RdmaCapableNV |";

        if (result.size() > 1)
        {
            result.pop_back();
            result.back() = '}';
        }
        else
        {
            result = "{}";
        }
        return result;
    } // VkSampleCountFlags
    static constexpr std::string to_string(VkSampleCountFlagBits flags)
    {
        std::string result = "{";
        if (flags & VK_SAMPLE_COUNT_1_BIT)
            result += " 1 |";
        if (flags & VK_SAMPLE_COUNT_2_BIT)
            result += " 2 |";
        if (flags & VK_SAMPLE_COUNT_4_BIT)
            result += " 4 |";
        if (flags & VK_SAMPLE_COUNT_8_BIT)
            result += " 8 |";
        if (flags & VK_SAMPLE_COUNT_16_BIT)
            result += " 16 |";
        if (flags & VK_SAMPLE_COUNT_32_BIT)
            result += " 32 |";
        if (flags & VK_SAMPLE_COUNT_64_BIT)
            result += " 64 |";
        if (result.size() > 1)
        {
            result.pop_back();
            result.back() = '}';
        }
        else
        {
            result = "{}";
        }
        return result;
    }

    static constexpr std::string to_string(const VkQueueFamilyProperties &prop)
    {
        return std::format("QueueFamilyProperties{{queueCount: {}, queueFlags: "
                           "{},timestampValidBits: {}, minImageTransferGranularity: {}}}",
                           prop.queueCount, to_string(prop.queueFlags),
                           to_string(prop.timestampValidBits),
                           to_string(prop.minImageTransferGranularity));
    }

    // NOLINTEND

}; // namespace mcs::vulkan