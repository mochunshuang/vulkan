#pragma once

#include "shader_module.hpp"

namespace mcs::vulkan::core
{
    struct create_shader_module // NOLINTBEGIN
    {
        auto create(const LogicalDevice *device)
        {
            VkShaderModuleCreateInfo createInfo_{.sType =
                                                     sType<VkShaderModuleCreateInfo>()};
            auto code = read_file(filePath);
            createInfo_.codeSize = code.size();
            createInfo_.pCode = std::bit_cast<const uint32_t *>(code.data());
            return shader_module{device, createInfo_};
        }

        /*
        typedef struct VkShaderModuleCreateInfo {
            VkStructureType              sType;
            const void*                  pNext;
            VkShaderModuleCreateFlags    flags;
            size_t                       codeSize;
            const uint32_t*              pCode;
        } VkShaderModuleCreateInfo;
        */
        const void *pNext{};
        VkShaderStageFlagBits flags{};
        std::string filePath{};
    }; // NOLINTEND

}; // namespace mcs::vulkan::core
