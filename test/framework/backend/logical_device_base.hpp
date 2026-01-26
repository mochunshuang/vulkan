#pragma once

#include "device_base.hpp"
#include "utils/check_vkresult.hpp"
#include <utility>
#include <vector>

namespace mcs::vulkan::core
{
    struct logical_device_base : private device_base
    {
        using device_base::device_base;

        constexpr value_type *operator->() noexcept
        {
            return &value_;
        }
        constexpr const value_type *operator->() const noexcept
        {
            return &value_;
        }
        constexpr explicit operator bool() const noexcept
        {
            return value_ != nullptr;
        }
        constexpr value_type &operator*() noexcept
        {
            return value_;
        }
        constexpr const value_type &operator*() const noexcept
        {
            return value_;
        }

        [[nodiscard]] constexpr VkQueue getDeviceQueue(uint32_t queueFamilyIndex,
                                                       uint32_t queueIndex) const
        {
            MCS_ASSERT(getDeviceQueue_ != nullptr);
            VkQueue queue; // NOLINT
            getDeviceQueue_(value_, queueFamilyIndex, queueIndex, &queue);
            return queue;
        }
        constexpr void destroyDevice(
            const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(destroyDevice_ != nullptr);
            destroyDevice_(value_, pAllocator);
        }

        constexpr void destroyImageView(
            VkImageView view, const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(destroyImageView_ != nullptr);
            destroyImageView_(value_, view, pAllocator);
        }
        [[nodiscard]] constexpr auto createImageView(
            const VkImageViewCreateInfo &createInfo,
            const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(createImageView_ != nullptr);
            VkImageView view; // NOLINT
            check_vkresult(createImageView_(value_, &createInfo, pAllocator, &view));
            return view;
        }

        constexpr void destroySwapchainKHR(
            VkSwapchainKHR &swapChain,
            const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(destroySwapchainKHR_ != nullptr);
            destroySwapchainKHR_(value_, swapChain, pAllocator);
        }

        [[nodiscard]] constexpr VkSwapchainKHR createSwapchainKHR(
            const VkSwapchainCreateInfoKHR &createInfo,
            const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(createSwapchainKHR_ != nullptr);
            VkSwapchainKHR swapChain; // NOLINT
            check_vkresult(
                createSwapchainKHR_(value_, &createInfo, pAllocator, &swapChain));
            return swapChain;
        }
        [[nodiscard]] constexpr std::vector<VkImage> getSwapchainImagesKHR(
            VkSwapchainKHR swapchain) const
        {
            MCS_ASSERT(getSwapchainImagesKHR_ != nullptr);
            uint32_t imageCount; // NOLINT
            check_vkresult(
                getSwapchainImagesKHR_(value_, swapchain, &imageCount, nullptr));
            std::vector<VkImage> associatedImages{imageCount};
            check_vkresult(getSwapchainImagesKHR_(value_, swapchain, &imageCount,
                                                  associatedImages.data()));
            return associatedImages;
        }
        [[nodiscard]] constexpr auto createPipelineLayout(
            const VkPipelineLayoutCreateInfo &createInfo,
            const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(createPipelineLayout_ != nullptr);
            VkPipelineLayout pipelineLayout; // NOLINT
            check_vkresult(
                createPipelineLayout_(value_, &createInfo, pAllocator, &pipelineLayout));
            return pipelineLayout;
        }
        constexpr void destroyPipelineLayout(
            VkPipelineLayout pipelineLayout,
            const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(destroyPipelineLayout_ != nullptr);
            destroyPipelineLayout_(value_, pipelineLayout, pAllocator);
        }

        [[nodiscard]] constexpr VkShaderModule createShaderModule(
            const VkShaderModuleCreateInfo &createInfo,
            const VkAllocationCallbacks *allocator) const
        {
            MCS_ASSERT(createShaderModule_ != nullptr);
            VkShaderModule shaderModule; // NOLINT
            check_vkresult(
                createShaderModule_(value_, &createInfo, allocator, &shaderModule));
            return shaderModule;
        }
        constexpr void destroyShaderModule(VkShaderModule shaderModule,
                                           const VkAllocationCallbacks *allocator) const
        {
            MCS_ASSERT(destroyShaderModule_ != nullptr);
            destroyShaderModule_(value_, shaderModule, allocator);
        }

        [[nodiscard]] constexpr VkPipeline createGraphicsPipelines(
            VkPipelineCache pipelineCache, uint32_t createInfoCount,
            const VkGraphicsPipelineCreateInfo &createInfos,
            const VkAllocationCallbacks *allocator) const
        {
            MCS_ASSERT(createGraphicsPipelines_ != nullptr);
            VkPipeline pipelines; // NOLINT
            check_vkresult(createGraphicsPipelines_(value_, pipelineCache,
                                                    createInfoCount, &createInfos,
                                                    allocator, &pipelines));
            return pipelines;
        }
        constexpr void destroyPipeline(
            VkPipeline pipelines, const VkAllocationCallbacks *allocator) const noexcept
        {
            MCS_ASSERT(destroyPipeline_ != nullptr);
            destroyPipeline_(value_, pipelines, allocator);
        }

        constexpr void waitIdle() const
        {
            MCS_ASSERT(deviceWaitIdle_ != nullptr);
            check_vkresult(deviceWaitIdle_(value_));
        }

        [[nodiscard]] constexpr VkCommandPool createCommandPool(
            const VkCommandPoolCreateInfo &poolInfo,
            const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(createCommandPool_ != nullptr);
            VkCommandPool command_pool; // NOLINT
            check_vkresult(
                createCommandPool_(value_, &poolInfo, pAllocator, &command_pool));
            return command_pool;
        }
        constexpr void destroyCommandPool(
            VkCommandPool command_pool,
            const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(destroyCommandPool_ != nullptr);
            destroyCommandPool_(value_, command_pool, pAllocator);
        }

        constexpr void allocateCommandBuffers(
            VkCommandBuffer &commandBuffer,
            const VkCommandBufferAllocateInfo &allocInfo) const
        {
            MCS_ASSERT(allocateCommandBuffers_ != nullptr);
            check_vkresult(allocateCommandBuffers_(value_, &allocInfo, &commandBuffer));
        }
        [[nodiscard]] constexpr auto allocateCommandBuffers(
            const VkCommandBufferAllocateInfo &allocInfo) const
        {
            MCS_ASSERT(allocateCommandBuffers_ != nullptr);
            std::vector<VkCommandBuffer> commandBuffer{allocInfo.commandBufferCount};
            check_vkresult(
                allocateCommandBuffers_(value_, &allocInfo, commandBuffer.data()));
            return commandBuffer;
        }
        constexpr void freeCommandBuffers(
            VkCommandPool commandPool, uint32_t commandBufferCount,
            const VkCommandBuffer &commandBuffers) const noexcept
        {
            MCS_ASSERT(freeCommandBuffers_ != nullptr);
            freeCommandBuffers_(value_, commandPool, commandBufferCount, &commandBuffers);
        }

        [[nodiscard]] constexpr auto createSemaphore(
            const VkSemaphoreCreateInfo &createInfo,
            const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(createSemaphore_ != nullptr);
            VkSemaphore semaphore; // NOLINT
            check_vkresult(createSemaphore_(value_, &createInfo, pAllocator, &semaphore));
            return semaphore;
        }
        constexpr void destroySemaphore(
            VkSemaphore semaphore, const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(destroySemaphore_ != nullptr);
            destroySemaphore_(value_, semaphore, pAllocator);
        }

        [[nodiscard]] constexpr auto createFence(
            const VkFenceCreateInfo &createInfo,
            const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(createFence_ != nullptr);
            VkFence fence; // NOLINT
            check_vkresult(createFence_(value_, &createInfo, pAllocator, &fence));
            return fence;
        }
        constexpr void destroyFence(
            VkFence fence, const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(destroyFence_ != nullptr);
            destroyFence_(value_, fence, pAllocator);
        }

        constexpr void beginCommandBuffer(VkCommandBuffer cmb,
                                          const VkCommandBufferBeginInfo &info) const
        {
            MCS_ASSERT(beginCommandBuffer_ != nullptr);
            check_vkresult(beginCommandBuffer_(cmb, &info));
        }
        constexpr void cmdBeginRendering(
            VkCommandBuffer commandBuffer,
            const VkRenderingInfo &renderingInfo) const noexcept
        {
            MCS_ASSERT(cmdBeginRendering_ != nullptr);
            cmdBeginRendering_(commandBuffer, &renderingInfo);
        }
        constexpr void cmdBindPipeline(VkCommandBuffer commandBuffer,
                                       VkPipelineBindPoint pipelineBindPoint,
                                       VkPipeline pipeline) const noexcept
        {
            MCS_ASSERT(cmdBindPipeline_ != nullptr);
            cmdBindPipeline_(commandBuffer, pipelineBindPoint, pipeline);
        }
        constexpr void cmdSetViewport(VkCommandBuffer commandBuffer,
                                      uint32_t firstViewport, uint32_t viewportCount,
                                      const VkViewport *pViewports) const noexcept
        {
            MCS_ASSERT(cmdSetViewport_ != nullptr);
            cmdSetViewport_(commandBuffer, firstViewport, viewportCount, pViewports);
        }
        constexpr void cmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor,
                                     uint32_t scissorCount,
                                     const VkRect2D *pScissors) const noexcept
        {
            MCS_ASSERT(cmdSetScissor_ != nullptr);
            cmdSetScissor_(commandBuffer, firstScissor, scissorCount, pScissors);
        }
        constexpr void cmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount,
                               uint32_t instanceCount, uint32_t firstVertex,
                               uint32_t firstInstance) const noexcept
        {
            MCS_ASSERT(cmdDraw_ != nullptr);
            cmdDraw_(commandBuffer, vertexCount, instanceCount, firstVertex,
                     firstInstance);
        }
        constexpr void cmdEndRendering(VkCommandBuffer commandBuffer) const noexcept
        {
            MCS_ASSERT(cmdEndRendering_ != nullptr);
            cmdEndRendering_(commandBuffer);
        }
        constexpr void endCommandBuffer(VkCommandBuffer commandBuffer) const
        {
            MCS_ASSERT(endCommandBuffer_ != nullptr);
            check_vkresult(endCommandBuffer_(commandBuffer));
        }
        [[nodiscard]] auto waitForFences(uint32_t fenceCount, const VkFence &fences,
                                         VkBool32 waitAll,
                                         uint64_t timeout) const noexcept
        {
            MCS_ASSERT(waitForFences_ != nullptr);
            return waitForFences_(value_, fenceCount, &fences, waitAll, timeout);
        }

        auto acquireNextImageKHR(VkSwapchainKHR swapchain, uint64_t timeout,
                                 VkSemaphore semaphore, VkFence fence) const noexcept
        {
            MCS_ASSERT(acquireNextImageKHR_ != nullptr);
            uint32_t index; // NOLINT
            return std::make_pair(acquireNextImageKHR_(value_, swapchain, timeout,
                                                       semaphore, fence, &index),
                                  index);
        }
        auto resetFences(uint32_t fenceCount, const VkFence &pFences) const
        {
            MCS_ASSERT(resetFences_ != nullptr);
            check_vkresult(resetFences_(value_, fenceCount, &pFences));
        }
        auto resetCommandBuffer(VkCommandBuffer commandBuffer,
                                VkCommandBufferResetFlagBits flags) const
        {
            MCS_ASSERT(resetCommandBuffer_ != nullptr);
            check_vkresult(resetCommandBuffer_(commandBuffer, flags));
        }

        auto queueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo &submits,
                         VkFence fence) const
        {
            MCS_ASSERT(queueSubmit_ != nullptr);
            check_vkresult(queueSubmit_(queue, submitCount, &submits, fence));
        }
        auto queuePresentKHR(VkQueue queue, const VkPresentInfoKHR &presentInfo) const
        {
            MCS_ASSERT(queuePresentKHR_ != nullptr);
            return queuePresentKHR_(queue, &presentInfo);
        }

        [[nodiscard]] auto createBuffer(const VkBufferCreateInfo &createInfo,
                                        const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(createBuffer_ != nullptr);
            VkBuffer buffer; // NOLINT
            check_vkresult(createBuffer_(value_, &createInfo, pAllocator, &buffer));
            return buffer;
        }

        [[nodiscard]] auto getBufferMemoryRequirements(VkBuffer buffer) const noexcept
        {
            MCS_ASSERT(getBufferMemoryRequirements_ != nullptr);
            VkMemoryRequirements requirements;
            getBufferMemoryRequirements_(value_, buffer, &requirements);
            return requirements;
        }

        [[nodiscard]] auto allocateMemory(const VkMemoryAllocateInfo &allocateInfo,
                                          const VkAllocationCallbacks *pAllocator) const
        {
            MCS_ASSERT(allocateMemory_ != nullptr);
            VkDeviceMemory memory; // NOLINT
            check_vkresult(allocateMemory_(value_, &allocateInfo, pAllocator, &memory));
            return memory;
        }
        void destroyBuffer(VkBuffer buffer,
                           const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(destroyBuffer_ != nullptr);
            destroyBuffer_(value_, buffer, pAllocator);
        }
        void bindBufferMemory(VkBuffer buffer, VkDeviceMemory memory,
                              VkDeviceSize memoryOffset) const
        {
            MCS_ASSERT(bindBufferMemory_ != nullptr);
            check_vkresult(bindBufferMemory_(value_, buffer, memory, memoryOffset));
        }
        void freeMemory(VkDeviceMemory memory,
                        const VkAllocationCallbacks *pAllocator) const noexcept
        {
            MCS_ASSERT(freeMemory_ != nullptr);
            freeMemory_(value_, memory, pAllocator);
        }
        void mapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                       VkMemoryMapFlags flags, void **ppData) const
        {
            MCS_ASSERT(mapMemory_ != nullptr);
            check_vkresult(mapMemory_(value_, memory, offset, size, flags, ppData));
        }
        void unmapMemory(VkDeviceMemory memory) const noexcept
        {
            MCS_ASSERT(unmapMemory_ != nullptr);
            unmapMemory_(value_, memory);
        }
        void cmdPipelineBarrier2(VkCommandBuffer commandBuffer,
                                 const VkDependencyInfo &dependencyInfo) const noexcept
        {
            MCS_ASSERT(cmdPipelineBarrier2_ != nullptr);
            cmdPipelineBarrier2_(commandBuffer, &dependencyInfo);
        }
        void cmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
                           VkBuffer dstBuffer,
                           const std::vector<VkBufferCopy> &regions) const noexcept
        {
            MCS_ASSERT(cmdCopyBuffer_ != nullptr);
            cmdCopyBuffer_(commandBuffer, srcBuffer, dstBuffer, regions.size(),
                           regions.data());
        }
        void queueWaitIdle(VkQueue queue) const noexcept
        {
            MCS_ASSERT(queueWaitIdle_ != nullptr);
            queueWaitIdle_(queue);
        }
        [[nodiscard]] constexpr auto getBufferDeviceAddress(
            const VkBufferDeviceAddressInfo &Info) const noexcept
        {
            MCS_ASSERT(getBufferDeviceAddress_ != nullptr);
            return getBufferDeviceAddress_(value_, &Info);
        }
        constexpr void cmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                          VkDeviceSize offset,
                                          VkIndexType indexType) const noexcept
        {
            cmdBindIndexBuffer_(commandBuffer, buffer, offset, indexType);
        }
        void cmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount,
                            uint32_t instanceCount, uint32_t firstIndex,
                            int32_t vertexOffset, uint32_t firstInstance) const noexcept
        {
            cmdDrawIndexed_(commandBuffer, indexCount, instanceCount, firstIndex,
                            vertexOffset, firstInstance);
        }
    };

}; // namespace mcs::vulkan::core