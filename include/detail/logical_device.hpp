#pragma once

#include "./vk_api/vk_logical_device_api.hpp"
#include "utils/vk_exception.hpp"
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace mcs::vulkan
{
    struct logical_device : vk_api::vk_logical_device_api
    {
        constexpr logical_device() = default;

        constexpr explicit logical_device(VkDevice device) noexcept : device_{device} {}

        constexpr ~logical_device() noexcept
        {
            destroy();
        }
        logical_device(const logical_device &) = delete;
        logical_device &operator=(const logical_device &) = delete;

        constexpr logical_device(logical_device &&other) noexcept
            : device_(std::exchange(other.device_, nullptr)) {};
        constexpr logical_device &operator=(logical_device &&other) noexcept
        {
            if (&other != this)
            {
                this->destroy();
                device_ = std::exchange(other.device_, nullptr);
            }
            return *this;
        };

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return device_ != nullptr;
        }
        [[nodiscard]] constexpr VkDevice raw_data() const noexcept // NOLINT
        {
            return device_;
        }

        // NOTE: the flags parameter of VkDeviceQueueCreateInfo set to zero
        // https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#:~:text=VkDeviceQueueCreateInfo%20set%20to-,zero,-.%20To%20get%20queues
        [[nodiscard]] VkQueue getDeviceQueue(uint32_t queueFamilyIndex,
                                             uint32_t queueIndex) const noexcept
        {
            return vk_api::vk_logical_device_api::getDeviceQueue(
                device_, queueFamilyIndex, queueIndex);
        }
        VkCommandPool createCommandPool(
            const VkCommandPoolCreateInfo &poolInfo,
            const VkAllocationCallbacks *pAllocator = nullptr) const
        {
            VkCommandPool command_pool; // NOLINT
            if (::vkCreateCommandPool(raw_data(), &poolInfo, pAllocator, &command_pool) !=
                VK_SUCCESS)
                throw utils::make_vk_exception("failed to create command pool!");
            return command_pool;
        }
        void destroyCommandPool(VkCommandPool command_pool) const noexcept
        {
            ::vkDestroyCommandPool(raw_data(), command_pool, nullptr);
        }

        [[nodiscard]] constexpr std::vector<VkImage> getSwapchainImagesKHR(
            VkSwapchainKHR &swapchain) const
        {
            uint32_t imageCount; // NOLINT
            std::vector<VkImage> associatedImages;
            ::vkGetSwapchainImagesKHR(device_, swapchain, &imageCount, nullptr);
            associatedImages.resize(imageCount);
            ::vkGetSwapchainImagesKHR(device_, swapchain, &imageCount,
                                      associatedImages.data());
            return associatedImages;
        }

        [[nodiscard]] VkSwapchainKHR createSwapchainKHR(
            const VkSwapchainCreateInfoKHR &createInfo,
            const VkAllocationCallbacks *pAllocator = nullptr) const
        {
            VkSwapchainKHR swapChain; // NOLINT
            if (::vkCreateSwapchainKHR(device_, &createInfo, pAllocator, &swapChain) !=
                VK_SUCCESS)
                throw utils::make_vk_exception("failed to create swap chain!");
            return swapChain;
        }
        void destroySwapchainKHR(
            VkSwapchainKHR &swapChain,
            const VkAllocationCallbacks *pAllocator = nullptr) const noexcept
        {
            ::vkDestroySwapchainKHR(device_, swapChain, pAllocator);
        }

        [[nodiscard]] VkImageView createImageView(
            const VkImageViewCreateInfo &createInfo,
            const VkAllocationCallbacks *pAllocator = nullptr) const
        {
            VkImageView view; // NOLINT
            if (vkCreateImageView(device_, &createInfo, pAllocator, &view) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to create image views!");
            return view;
        }
        void destroyImageView(VkImageView view, const VkAllocationCallbacks *pAllocator =
                                                    nullptr) const noexcept
        {
            ::vkDestroyImageView(device_, view, pAllocator);
        }

        [[nodiscard]] VkImage createImage(
            const VkImageCreateInfo &imageInfo,
            const VkAllocationCallbacks *pAllocator = nullptr) const
        {
            VkImage image; // NOLINT
            if (::vkCreateImage(device_, &imageInfo, pAllocator, &image) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to create image!");
            return image;
        }
        void destroyImage(VkImage image, const VkAllocationCallbacks *pAllocator =
                                             nullptr) const noexcept
        {
            ::vkDestroyImage(device_, image, pAllocator);
        }
        [[nodiscard]] VkMemoryRequirements getImageMemoryRequirements(
            VkImage image) const noexcept
        {
            VkMemoryRequirements memRequirements;
            ::vkGetImageMemoryRequirements(device_, image, &memRequirements);
            return memRequirements;
        }

        [[nodiscard]] VkDeviceMemory allocateMemory(
            const VkMemoryAllocateInfo &allocateInfo,
            const VkAllocationCallbacks *pAllocator = nullptr) const
        {
            VkDeviceMemory memory; // NOLINT
            if (::vkAllocateMemory(device_, &allocateInfo, pAllocator, &memory) !=
                VK_SUCCESS)
                throw utils::make_vk_exception("failed to allocate image memory!");
            return memory;
        }
        void freeMemory(VkDeviceMemory memory,
                        const VkAllocationCallbacks *pAllocator = nullptr) const noexcept
        {
            ::vkFreeMemory(device_, memory, pAllocator);
        }

        void bindImageMemory(VkImage image, VkDeviceMemory imageMemory,
                             VkDeviceSize memoryOffset) const
        {
            if (::vkBindImageMemory(device_, image, imageMemory, memoryOffset) !=
                VK_SUCCESS)
                throw utils::make_vk_exception("failed to bind image memory!");
        }

        void mapMempry(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                       VkMemoryMapFlags flags, void **pdata) const
        {
            if (::vkMapMemory(device_, memory, offset, size, flags, pdata) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to map mempry!");
        }
        void unmapMemory(VkDeviceMemory memory) const noexcept
        {
            ::vkUnmapMemory(device_, memory);
        }

        [[nodiscard]] VkBuffer createBuffer(
            const VkBufferCreateInfo &createInfo,
            const VkAllocationCallbacks *pAllocator = nullptr) const
        {
            VkBuffer buffer; // NOLINT
            if (vkCreateBuffer(device_, &createInfo, pAllocator, &buffer) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to create buffer!");
            return buffer;
        }
        void destroyBuffer(VkBuffer buffer, const VkAllocationCallbacks *allocator =
                                                nullptr) const noexcept
        {
            ::vkDestroyBuffer(device_, buffer, allocator);
        }

        VkMemoryRequirements getBufferMemoryRequirements(VkBuffer buffer) const noexcept
        {
            VkMemoryRequirements memRequirements;
            ::vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);
            return memRequirements;
        }

        void bindBufferMemory(VkBuffer buffer, VkDeviceMemory bufferMemory,
                              VkDeviceSize memoryOffset) const
        {
            if (::vkBindBufferMemory(device_, buffer, bufferMemory, memoryOffset) !=
                VK_SUCCESS)
                throw utils::make_vk_exception("failed to bind buffer memory!");
        }

        [[nodiscard]] VkCommandBuffer allocateCommandBuffers(
            const VkCommandBufferAllocateInfo &allocInfo) const
        {
            VkCommandBuffer commandBuffer; // NOLINT
            if (::vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer) !=
                VK_SUCCESS)
                throw utils::make_vk_exception("failed to bind buffer memory!");
            return commandBuffer;
        }
        void allocateCommandBuffers(VkCommandBuffer &commandBuffer,
                                    const VkCommandBufferAllocateInfo &allocInfo) const
        {
            if (::vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer) !=
                VK_SUCCESS)
                throw utils::make_vk_exception("failed to bind buffer memory!");
        }
        void freeCommandBuffers(VkCommandPool commandPool, uint32_t commandBufferCount,
                                const VkCommandBuffer &commandBuffers) const noexcept
        {
            ::vkFreeCommandBuffers(device_, commandPool, commandBufferCount,
                                   &commandBuffers);
        }
        // vkCreateSampler
        [[nodiscard]] VkSampler createSampler(
            const VkSamplerCreateInfo &createInfo,
            const VkAllocationCallbacks *pAllocator = nullptr) const
        {
            VkSampler sampler; // NOLINT
            if (::vkCreateSampler(device_, &createInfo, pAllocator, &sampler) !=
                VK_SUCCESS)
                throw utils::make_vk_exception("failed to create texture sampler view!");
            return sampler;
        }
        void destroySampler(VkSampler sampler,
                            const VkAllocationCallbacks *pAllocator = nullptr) const
        {
            ::vkDestroySampler(device_, sampler, pAllocator);
        }

        void waitIdle() const
        {
            if (::vkDeviceWaitIdle(device_) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to vkDeviceWaitIdle()!");
        }

        [[nodiscard]] VkDescriptorPool createDescriptorPool(
            const VkDescriptorPoolCreateInfo &createInfo,
            const VkAllocationCallbacks *allocator = nullptr) const
        {
            VkDescriptorPool descriptorPool; // NOLINT
            if (::vkCreateDescriptorPool(device_, &createInfo, allocator,
                                         &descriptorPool) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to create descriptor pool!");
            return descriptorPool;
        }
        void destroyDescriptorPool(
            VkDescriptorPool descriptorPool,
            const VkAllocationCallbacks *allocator = nullptr) const noexcept
        {
            ::vkDestroyDescriptorPool(device_, descriptorPool, allocator);
        }

        auto createDescriptorSetLayout(
            const VkDescriptorSetLayoutCreateInfo &createInfo,
            const VkAllocationCallbacks *pAllocator = nullptr) const
        {
            VkDescriptorSetLayout setLayout; // NOLINT
            if (::vkCreateDescriptorSetLayout(device_, &createInfo, pAllocator,
                                              &setLayout) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to create descriptor set layout!");
            return setLayout;
        }
        void destroyDescriptorSetLayout(
            VkDescriptorSetLayout descriptorSetLayout,
            const VkAllocationCallbacks *allocator = nullptr) const noexcept
        {
            ::vkDestroyDescriptorSetLayout(device_, descriptorSetLayout, allocator);
        }

        [[nodiscard]] auto allocateDescriptorSets(
            const VkDescriptorSetAllocateInfo &allocateInfo) const
        {
            std::vector<VkDescriptorSet> descriptorSets(allocateInfo.descriptorSetCount);
            if (::vkAllocateDescriptorSets(device_, &allocateInfo,
                                           descriptorSets.data()) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to allocate descriptor sets!");
            return descriptorSets;
        }

        [[nodiscard]] VkShaderModule createShaderModule(
            const VkShaderModuleCreateInfo &createInfo,
            const VkAllocationCallbacks *allocator = nullptr) const
        {
            VkShaderModule shaderModule; // NOLINT
            if (::vkCreateShaderModule(device_, &createInfo, allocator, &shaderModule) !=
                VK_SUCCESS)
                throw utils::make_vk_exception("failed to create shader module!");
            return shaderModule;
        }
        void destroyShaderModule(VkShaderModule shaderModule,
                                 const VkAllocationCallbacks *allocator = nullptr) const
        {
            ::vkDestroyShaderModule(device_, shaderModule, allocator);
        }

        [[nodiscard]] VkPipeline createGraphicsPipelines(
            VkPipelineCache pipelineCache, uint32_t createInfoCount,
            const VkGraphicsPipelineCreateInfo &createInfos,
            const VkAllocationCallbacks *allocator = nullptr) const
        {
            VkPipeline pipelines; // NOLINT
            if (::vkCreateGraphicsPipelines(device_, pipelineCache, createInfoCount,
                                            &createInfos, allocator,
                                            &pipelines) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to create graphics pipeline!");
            return pipelines;
        }
        void destroyPipeline(
            VkPipeline pipelines,
            const VkAllocationCallbacks *allocator = nullptr) const noexcept
        {
            ::vkDestroyPipeline(device_, pipelines, allocator);
        }

        [[nodiscard]] VkPipelineLayout createPipelineLayout(
            const VkPipelineLayoutCreateInfo &createInfo,
            const VkAllocationCallbacks *allocator = nullptr) const
        {
            VkPipelineLayout pipelineLayout; // NOLINT
            if (::vkCreatePipelineLayout(device_, &createInfo, allocator,
                                         &pipelineLayout) != VK_SUCCESS)
                throw utils::make_vk_exception("failed to create pipeline layout!");
            return pipelineLayout;
        }
        void destroyPipelineLayout(
            VkPipelineLayout pipelineLayout,
            const VkAllocationCallbacks *allocator = nullptr) const noexcept
        {
            ::vkDestroyPipelineLayout(device_, pipelineLayout, allocator);
        }
        void updateDescriptorSets(
            uint32_t descriptorWriteCount, const VkWriteDescriptorSet *descriptorWrites,
            uint32_t descriptorCopyCount,
            const VkCopyDescriptorSet *descriptorCopies) const noexcept
        {
            ::vkUpdateDescriptorSets(device_, descriptorWriteCount, descriptorWrites,
                                     descriptorCopyCount, descriptorCopies);
        }

      private:
        VkDevice device_ = nullptr;

        constexpr void destroy() noexcept
        {
            if (device_ != nullptr)
            {
                ::vkDestroyDevice(device_, nullptr);
                device_ = nullptr;
            }
        }
    };
}; // namespace mcs::vulkan