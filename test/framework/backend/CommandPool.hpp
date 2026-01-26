#pragma once

#include "./LogicalDevice.hpp"
#include "sType.hpp"
#include "utils/mcs_assert.hpp"
#include <cstddef>
#include <utility>
#include <vector>

#include <ranges>

namespace mcs::vulkan::core
{
    class CommandBuffer;
    class CommandPool
    {
        const LogicalDevice *logicalDevice_{};
        VkCommandPool value_{};

        friend CommandBuffer;

      public:
        constexpr explicit operator bool() const noexcept
        {
            return value_ != nullptr;
        }
        constexpr auto &operator*() noexcept
        {
            return value_;
        }
        constexpr const auto &operator*() const noexcept
        {
            return value_;
        }

        /*
        typedef struct VkCommandPoolCreateInfo {
            VkStructureType             sType;
            const void*                 pNext;
            VkCommandPoolCreateFlags    flags;
            uint32_t                    queueFamilyIndex;
        } VkCommandPoolCreateInfo;
        */
        struct config_command_pool
        {
            const void *pNext{};                 // NOLINT
            VkCommandPoolCreateFlagBits flags{}; // NOLINT
            uint32_t queueFamilyIndex{};         // NOLINT
            [[nodiscard]] constexpr VkCommandPoolCreateInfo create() const noexcept
            {
                return {.sType = sType<VkCommandPoolCreateInfo>(),
                        .pNext = pNext,
                        .flags = static_cast<VkCommandPoolCreateFlags>(flags),
                        .queueFamilyIndex = queueFamilyIndex};
            }
        };
        CommandPool() = default;
        constexpr CommandPool(const LogicalDevice &device,
                              const config_command_pool &createInfo)
            : logicalDevice_{&device},
              value_{device.createCommandPool(createInfo.create(), device.allocator())}
        {
        }
        [[nodiscard]] constexpr const LogicalDevice *logicalDevice() const noexcept
        {
            return logicalDevice_;
        }
        constexpr void clear() noexcept
        {
            if (value_ != nullptr)
                logicalDevice_->destroyCommandPool(value_, logicalDevice_->allocator());
            value_ = nullptr;
        }
        /*
        typedef struct VkCommandBufferAllocateInfo {
            VkStructureType         sType;
            const void*             pNext;
            VkCommandPool           commandPool;
            VkCommandBufferLevel    level;
            uint32_t                commandBufferCount;
        } VkCommandBufferAllocateInfo;
        */
        struct config_command_buffer
        {
            [[nodiscard]] VkCommandBufferAllocateInfo create(
                VkCommandPool commandPool) const noexcept
            {
                return {.sType = sType<VkCommandBufferAllocateInfo>(),
                        .pNext = pNext,
                        .commandPool = commandPool,
                        .level = level,
                        .commandBufferCount = commandBufferCount};
            } // NOLINTBEGIN
            const void *pNext{};
            VkCommandBufferLevel level{};
            uint32_t commandBufferCount{}; // NOLINTEND
        };

        [[nodiscard]] constexpr std::vector<CommandBuffer> allocateCommandBuffers(
            const config_command_buffer &config) const;

        [[nodiscard]] constexpr CommandBuffer allocateOneCommandBuffer(
            const config_command_buffer &config) const;
    };

    class CommandBuffer
    {
        using value_type = VkCommandBuffer;
        const CommandPool *pool_{};
        VkCommandBuffer value_{};

      public:
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

        CommandBuffer() = default;
        CommandBuffer(const CommandBuffer &) = delete;
        constexpr CommandBuffer(CommandBuffer &&other) noexcept
            : pool_{std::exchange(other.pool_, {})},
              value_{std::exchange(other.value_, {})} {

              };
        CommandBuffer &operator=(const CommandBuffer &) = delete;
        CommandBuffer &operator=(CommandBuffer &&other) noexcept
        {
            if (&other != this)
            {
                this->clear();
                pool_ = std::exchange(other.pool_, {});
                value_ = std::exchange(other.value_, {});
            }
            return *this;
        };

        constexpr explicit CommandBuffer(const CommandPool *pool,
                                         VkCommandBuffer value) noexcept
            : pool_{pool}, value_{value}
        {
        }
        constexpr ~CommandBuffer() noexcept
        {
            clear();
        }
        constexpr void clear() noexcept
        {
            if (value_ != nullptr)
            {
                pool_->logicalDevice_->freeCommandBuffers(*(*pool_), 1, value_);
                value_ = nullptr;
                pool_ = nullptr;
            }
        }

        struct begin_info
        {
            /*
            typedef struct VkCommandBufferBeginInfo {
                VkStructureType                          sType;
                const void*                              pNext;
                VkCommandBufferUsageFlags                flags;
                const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
            } VkCommandBufferBeginInfo;
            */
            [[nodiscard]] VkCommandBufferBeginInfo create() const noexcept
            {
                pInheritanceInfo.sType = sType<VkCommandBufferInheritanceInfo>();
                return {.sType = sType<VkCommandBufferBeginInfo>(),
                        .pNext = pNext,
                        .flags = static_cast<VkCommandBufferUsageFlags>(flags),
                        .pInheritanceInfo = &pInheritanceInfo};
            }
            const void *pNext{};                                       // NOLINT
            VkCommandBufferUsageFlagBits flags{};                      // NOLINT
            mutable VkCommandBufferInheritanceInfo pInheritanceInfo{}; // NOLINT
        };
        constexpr void begin(const begin_info &beginInfo) const
        {
            pool_->logicalDevice_->beginCommandBuffer(value_, beginInfo.create());
        }

        constexpr void beginRendering(const VkRenderingInfo &info) const noexcept
        {
            pool_->logicalDevice_->cmdBeginRendering(value_, info);
        }
        constexpr void bindPipeline(VkPipelineBindPoint pipelineBindPoint,
                                    VkPipeline pipeline) const noexcept
        {
            pool_->logicalDevice_->cmdBindPipeline(value_, pipelineBindPoint, pipeline);
        }
        constexpr void setViewport(uint32_t firstViewport,
                                   std::vector<VkViewport> viewports) const noexcept
        {
            pool_->logicalDevice_->cmdSetViewport(value_, firstViewport, viewports.size(),
                                                  viewports.data());
        }
        constexpr void setScissor(uint32_t firstScissor,
                                  std::vector<VkRect2D> scissors) const noexcept
        {
            pool_->logicalDevice_->cmdSetScissor(value_, firstScissor, scissors.size(),
                                                 scissors.data());
        }
        constexpr void draw(uint32_t vertexCount, uint32_t instanceCount,
                            uint32_t firstVertex, uint32_t firstInstance) const noexcept
        {
            pool_->logicalDevice_->cmdDraw(value_, vertexCount, instanceCount,
                                           firstVertex, firstInstance);
        }
        constexpr void endRendering() const noexcept
        {
            pool_->logicalDevice_->cmdEndRendering(value_);
        }
        constexpr void end() const
        {
            pool_->logicalDevice_->endCommandBuffer(value_);
        }
        constexpr void reset(VkCommandBufferResetFlagBits flags) const
        {
            pool_->logicalDevice_->resetCommandBuffer(value_, flags);
        }
        constexpr void pipelineBarrier2(
            const VkDependencyInfo &dependencyInfo) const noexcept
        {
            pool_->logicalDevice_->cmdPipelineBarrier2(value_, dependencyInfo);
        }
        constexpr void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                                  const std::vector<VkBufferCopy> &regions) const noexcept
        {
            pool_->logicalDevice_->cmdCopyBuffer(value_, srcBuffer, dstBuffer, regions);
        }
        constexpr void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset,
                                       VkIndexType indexType) const noexcept
        {
            pool_->logicalDevice_->cmdBindIndexBuffer(value_, buffer, offset, indexType);
        }
        void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                         int32_t vertexOffset, uint32_t firstInstance) const noexcept
        {
            pool_->logicalDevice_->cmdDrawIndexed(value_, indexCount, instanceCount,
                                                  firstIndex, vertexOffset,
                                                  firstInstance);
        }
    };

    constexpr std::vector<CommandBuffer> CommandPool::allocateCommandBuffers(
        const config_command_buffer &config) const
    {
        return logicalDevice_->allocateCommandBuffers(config.create(value_)) |
               std::views::transform([this](VkCommandBuffer cb) constexpr noexcept {
                   return CommandBuffer{this, cb};
               }) |
               std::ranges::to<std::vector<CommandBuffer>>();
    }

    [[nodiscard]] constexpr CommandBuffer CommandPool::allocateOneCommandBuffer(
        const config_command_buffer &config) const
    {
        VkCommandBuffer cb; // NOLINT
        // config.commandBufferCount = 1;
        MCS_ASSERT(config.commandBufferCount == 1);
        logicalDevice_->allocateCommandBuffers(cb, config.create(value_));
        return CommandBuffer{this, cb};
    }

}; // namespace mcs::vulkan::core