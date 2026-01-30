#pragma once

#include "proc_addr.hpp"

#include "utils/mcs_assert.hpp"
#include "utils/mcslog.hpp"

namespace mcs::vulkan::core
{
    // NOLINTBEGIN
    struct device_base
    {
      private:
        template <typename Fn>
        constexpr auto funPtr(const char *pName) noexcept
        {
            return proc_addr<Fn>(value_, pName);
        }

      public:
        // NOLINTBEGIN
        using value_type = VkDevice;
        VkDevice value_{};
        PFN_vkGetDeviceQueue getDeviceQueue_{};
        PFN_vkDestroyDevice destroyDevice_{};
        PFN_vkCreateImageView createImageView_{};
        PFN_vkDestroyImageView destroyImageView_{};
        PFN_vkDestroySwapchainKHR destroySwapchainKHR_{};
        PFN_vkCreateSwapchainKHR createSwapchainKHR_{};
        PFN_vkGetSwapchainImagesKHR getSwapchainImagesKHR_{};
        PFN_vkCreatePipelineLayout createPipelineLayout_{};
        PFN_vkDestroyPipelineLayout destroyPipelineLayout_{};
        PFN_vkCreateShaderModule createShaderModule_{};
        PFN_vkDestroyShaderModule destroyShaderModule_{};
        PFN_vkCreateGraphicsPipelines createGraphicsPipelines_{};
        PFN_vkDestroyPipeline destroyPipeline_{};
        PFN_vkDeviceWaitIdle deviceWaitIdle_{};
        PFN_vkCreateCommandPool createCommandPool_{};
        PFN_vkDestroyCommandPool destroyCommandPool_{};
        PFN_vkAllocateCommandBuffers allocateCommandBuffers_{};
        PFN_vkFreeCommandBuffers freeCommandBuffers_{};
        PFN_vkCreateSemaphore createSemaphore_{};
        PFN_vkDestroySemaphore destroySemaphore_{};
        PFN_vkCreateFence createFence_{};
        PFN_vkDestroyFence destroyFence_{};
        PFN_vkBeginCommandBuffer beginCommandBuffer_{};
        PFN_vkCmdBeginRendering cmdBeginRendering_{};
        PFN_vkCmdBindPipeline cmdBindPipeline_{};
        PFN_vkCmdSetViewport cmdSetViewport_{};
        PFN_vkCmdSetScissor cmdSetScissor_{};
        PFN_vkCmdDraw cmdDraw_{};
        PFN_vkCmdEndRendering cmdEndRendering_{};
        PFN_vkEndCommandBuffer endCommandBuffer_{};
        PFN_vkWaitForFences waitForFences_{};
        PFN_vkAcquireNextImageKHR acquireNextImageKHR_{};
        PFN_vkResetFences resetFences_{};
        PFN_vkResetCommandBuffer resetCommandBuffer_{};
        PFN_vkQueueSubmit queueSubmit_{};
        PFN_vkQueuePresentKHR queuePresentKHR_{};
        PFN_vkCreateBuffer createBuffer_{};
        PFN_vkGetBufferMemoryRequirements getBufferMemoryRequirements_{};
        PFN_vkAllocateMemory allocateMemory_{};
        PFN_vkDestroyBuffer destroyBuffer_{};
        PFN_vkBindBufferMemory bindBufferMemory_{};
        PFN_vkFreeMemory freeMemory_{};
        PFN_vkMapMemory mapMemory_{};
        PFN_vkUnmapMemory unmapMemory_{};
        PFN_vkCmdPipelineBarrier2 cmdPipelineBarrier2_{};
        PFN_vkCmdCopyBuffer cmdCopyBuffer_{};
        PFN_vkQueueWaitIdle queueWaitIdle_{};
        PFN_vkGetBufferDeviceAddress getBufferDeviceAddress_{};
        PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer_{};
        PFN_vkCmdDrawIndexed cmdDrawIndexed_{};
        PFN_vkCmdPushConstants cmdPushConstants_{};
        PFN_vkCreateDescriptorSetLayout createDescriptorSetLayout_{};
        PFN_vkCreateDescriptorPool createDescriptorPool_;
        PFN_vkAllocateDescriptorSets allocateDescriptorSets_{};
        PFN_vkUpdateDescriptorSets updateDescriptorSets_{};
        PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets_{};
        PFN_vkDestroyDescriptorPool destroyDescriptorPool_{};
        PFN_vkDestroyDescriptorSetLayout destroyDescriptorSetLayout_{};
        PFN_vkFreeDescriptorSets freeDescriptorSets_{};
        PFN_vkCreateImage createImage_{};
        PFN_vkGetImageMemoryRequirements getImageMemoryRequirements_{};
        PFN_vkCmdCopyBufferToImage cmdCopyBufferToImage_{};
        PFN_vkCreateSampler createSampler_{};
        PFN_vkDestroySampler destroySampler_{};
        PFN_vkDestroyImage destroyImage_{};
        PFN_vkBindImageMemory bindImageMemory_{};
        // NOLINTEND
        device_base() = default;
        constexpr explicit device_base(value_type value) noexcept
            : value_{value},
              getDeviceQueue_{funPtr<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")},
              destroyDevice_{funPtr<PFN_vkDestroyDevice>("vkDestroyDevice")},
              createImageView_{funPtr<PFN_vkCreateImageView>("vkCreateImageView")},
              destroyImageView_{funPtr<PFN_vkDestroyImageView>("vkDestroyImageView")},
              destroySwapchainKHR_{
                  funPtr<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR")},
              createSwapchainKHR_{
                  funPtr<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")},
              getSwapchainImagesKHR_{
                  funPtr<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR")},
              createPipelineLayout_{
                  funPtr<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")},
              destroyPipelineLayout_{
                  funPtr<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")},
              createShaderModule_{
                  funPtr<PFN_vkCreateShaderModule>("vkCreateShaderModule")},
              destroyShaderModule_{
                  funPtr<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")},
              createGraphicsPipelines_{
                  funPtr<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")},
              destroyPipeline_{funPtr<PFN_vkDestroyPipeline>("vkDestroyPipeline")},
              deviceWaitIdle_{funPtr<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")},
              createCommandPool_{funPtr<PFN_vkCreateCommandPool>("vkCreateCommandPool")},
              destroyCommandPool_{
                  funPtr<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")},
              allocateCommandBuffers_{
                  funPtr<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")},
              freeCommandBuffers_{
                  funPtr<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")},
              createSemaphore_{funPtr<PFN_vkCreateSemaphore>("vkCreateSemaphore")},
              destroySemaphore_{funPtr<PFN_vkDestroySemaphore>("vkDestroySemaphore")},
              createFence_{funPtr<PFN_vkCreateFence>("vkCreateFence")},
              destroyFence_{funPtr<PFN_vkDestroyFence>("vkDestroyFence")},
              beginCommandBuffer_{
                  funPtr<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")},
              cmdBeginRendering_{funPtr<PFN_vkCmdBeginRendering>("vkCmdBeginRendering")},
              cmdBindPipeline_{funPtr<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")},
              cmdSetViewport_{funPtr<PFN_vkCmdSetViewport>("vkCmdSetViewport")},
              cmdSetScissor_{funPtr<PFN_vkCmdSetScissor>("vkCmdSetScissor")},
              cmdDraw_{funPtr<PFN_vkCmdDraw>("vkCmdDraw")},
              cmdEndRendering_{funPtr<PFN_vkCmdEndRendering>("vkCmdEndRendering")},
              endCommandBuffer_{funPtr<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")},
              waitForFences_{funPtr<PFN_vkWaitForFences>("vkWaitForFences")},
              acquireNextImageKHR_{
                  funPtr<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR")},
              resetFences_{funPtr<PFN_vkResetFences>("vkResetFences")},
              resetCommandBuffer_{
                  funPtr<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")},
              queueSubmit_{funPtr<PFN_vkQueueSubmit>("vkQueueSubmit")},
              queuePresentKHR_{funPtr<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")},
              createBuffer_{funPtr<PFN_vkCreateBuffer>("vkCreateBuffer")},
              getBufferMemoryRequirements_{funPtr<PFN_vkGetBufferMemoryRequirements>(
                  "vkGetBufferMemoryRequirements")},
              allocateMemory_{funPtr<PFN_vkAllocateMemory>("vkAllocateMemory")},
              destroyBuffer_{funPtr<PFN_vkDestroyBuffer>("vkDestroyBuffer")},
              bindBufferMemory_{funPtr<PFN_vkBindBufferMemory>("vkBindBufferMemory")},
              freeMemory_{funPtr<PFN_vkFreeMemory>("vkFreeMemory")},
              mapMemory_{funPtr<PFN_vkMapMemory>("vkMapMemory")},
              unmapMemory_{funPtr<PFN_vkUnmapMemory>("vkUnmapMemory")},
              cmdPipelineBarrier2_{
                  funPtr<PFN_vkCmdPipelineBarrier2>("vkCmdPipelineBarrier2")},
              cmdCopyBuffer_{funPtr<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")},
              queueWaitIdle_{funPtr<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")},
              getBufferDeviceAddress_{
                  funPtr<PFN_vkGetBufferDeviceAddress>("vkGetBufferDeviceAddress")},
              cmdBindIndexBuffer_{
                  funPtr<PFN_vkCmdBindIndexBuffer>("vkCmdBindIndexBuffer")},
              cmdDrawIndexed_{funPtr<PFN_vkCmdDrawIndexed>("vkCmdDrawIndexed")},
              cmdPushConstants_{funPtr<PFN_vkCmdPushConstants>("vkCmdPushConstants")},
              createDescriptorSetLayout_{
                  funPtr<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")},
              createDescriptorPool_{
                  funPtr<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")},
              allocateDescriptorSets_{
                  funPtr<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")},
              updateDescriptorSets_{
                  funPtr<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")},
              cmdBindDescriptorSets_{
                  funPtr<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")},
              destroyDescriptorPool_{
                  funPtr<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")},
              destroyDescriptorSetLayout_{funPtr<PFN_vkDestroyDescriptorSetLayout>(
                  "vkDestroyDescriptorSetLayout")},
              freeDescriptorSets_{
                  funPtr<PFN_vkFreeDescriptorSets>("vkFreeDescriptorSets")},
              createImage_{funPtr<PFN_vkCreateImage>("vkCreateImage")},
              getImageMemoryRequirements_{funPtr<PFN_vkGetImageMemoryRequirements>(
                  "vkGetImageMemoryRequirements")},
              cmdCopyBufferToImage_{
                  funPtr<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")},
              createSampler_{funPtr<PFN_vkCreateSampler>("vkCreateSampler")},
              destroySampler_{funPtr<PFN_vkDestroySampler>("vkDestroySampler")},
              destroyImage_{funPtr<PFN_vkDestroyImage>("vkDestroyImage")},
              bindImageMemory_{funPtr<PFN_vkBindImageMemory>("vkBindImageMemory")}
        {
            MCSLOG_INFO("load VkDevice pfn [begin]");
            MCS_ASSERT(getDeviceQueue_ != nullptr);
            MCS_ASSERT(destroyDevice_ != nullptr);
            MCS_ASSERT(createImageView_ != nullptr);
            MCS_ASSERT(destroyImageView_ != nullptr);
            MCS_ASSERT(destroySwapchainKHR_ != nullptr);
            MCS_ASSERT(createSwapchainKHR_ != nullptr);
            MCS_ASSERT(createPipelineLayout_ != nullptr);
            MCS_ASSERT(createShaderModule_ != nullptr);
            MCS_ASSERT(destroyShaderModule_ != nullptr);
            MCS_ASSERT(createGraphicsPipelines_ != nullptr);
            MCS_ASSERT(destroyPipeline_ != nullptr);
            MCS_ASSERT(createCommandPool_ != nullptr);
            MCS_ASSERT(destroyCommandPool_ != nullptr);
            MCS_ASSERT(allocateCommandBuffers_ != nullptr);
            MCS_ASSERT(freeCommandBuffers_ != nullptr);
            MCS_ASSERT(createSemaphore_ != nullptr);
            MCS_ASSERT(destroySemaphore_ != nullptr);
            MCS_ASSERT(createFence_ != nullptr);
            MCS_ASSERT(destroyFence_ != nullptr);
            MCS_ASSERT(beginCommandBuffer_ != nullptr);
            MCS_ASSERT(cmdBeginRendering_ != nullptr);
            MCS_ASSERT(cmdBindPipeline_ != nullptr);
            MCS_ASSERT(cmdSetViewport_ != nullptr);
            MCS_ASSERT(cmdSetScissor_ != nullptr);
            MCS_ASSERT(cmdDraw_ != nullptr);
            MCS_ASSERT(cmdEndRendering_ != nullptr);
            MCS_ASSERT(endCommandBuffer_ != nullptr);
            MCS_ASSERT(waitForFences_ != nullptr);
            MCS_ASSERT(acquireNextImageKHR_ != nullptr);
            MCS_ASSERT(resetFences_ != nullptr);
            MCS_ASSERT(resetCommandBuffer_ != nullptr);
            MCS_ASSERT(queueSubmit_ != nullptr);
            MCS_ASSERT(queuePresentKHR_ != nullptr);
            MCS_ASSERT(createBuffer_ != nullptr);
            MCS_ASSERT(getBufferMemoryRequirements_ != nullptr);
            MCS_ASSERT(allocateMemory_ != nullptr);
            MCS_ASSERT(destroyBuffer_ != nullptr);
            MCS_ASSERT(bindBufferMemory_ != nullptr);
            MCS_ASSERT(freeMemory_ != nullptr);
            MCS_ASSERT(mapMemory_ != nullptr);
            MCS_ASSERT(unmapMemory_ != nullptr);
            MCS_ASSERT(cmdPipelineBarrier2_ != nullptr);
            MCS_ASSERT(cmdCopyBuffer_ != nullptr);
            MCS_ASSERT(queueWaitIdle_ != nullptr);
            MCS_ASSERT(getBufferDeviceAddress_ != nullptr);
            MCSLOG_INFO("load VkDevice pfn [end]");
        }
    };
    // NOLINTEND

} // namespace mcs::vulkan::core