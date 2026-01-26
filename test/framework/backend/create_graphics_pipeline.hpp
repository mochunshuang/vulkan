#pragma once

#include "LogicalDevice.hpp"
#include "sType.hpp"
#include <optional>
#include <utility>
#include <vector>

#include "create_shader_module.hpp"
#include "structure_chain.hpp"
#include "utils/mcs_assert.hpp"

namespace mcs::vulkan::core
{
    struct create_graphics_pipeline
    {
        explicit create_graphics_pipeline(const LogicalDevice &device) noexcept
            : logicalDevice_{&device}
        {
        }

        //-----------------config--------------------
        struct config_shader_stage // NOLINTBEGIN
        {
            auto createShaders(const LogicalDevice *logicalDevice_)
            {
                return shader_info.create(logicalDevice_);
            }

            VkPipelineShaderStageCreateInfo create() noexcept
            {
                // NOTE: Be careful: NOT SET .module
                return {.sType = sType<VkPipelineShaderStageCreateInfo>(),
                        .pNext = pNext,
                        .flags = static_cast<VkPipelineShaderStageCreateFlags>(flags),
                        .stage = stage,
                        .pName = pName,
                        .pSpecializationInfo =
                            &pSpecializationInfo}; // NOTE: 0初始化不影响
            }

            /*
            typedef struct VkPipelineShaderStageCreateInfo {
                VkStructureType                     sType;
                const void*                         pNext;
                VkPipelineShaderStageCreateFlags    flags;
                VkShaderStageFlagBits               stage;
                VkShaderModule                      module;
                const char*                         pName;
                const VkSpecializationInfo*         pSpecializationInfo;
            } VkPipelineShaderStageCreateInfo;
            */
            const void *pNext{};
            VkPipelineShaderStageCreateFlagBits flags{};
            VkShaderStageFlagBits stage{};
            const char *pName{};
            VkSpecializationInfo pSpecializationInfo{};
            create_shader_module shader_info{}; // NOLINTEND
        };
        auto &configShaderStage(std::vector<config_shader_stage> shaders) noexcept
        {
            configShaderStages_ = std::move(shaders);
            return *this;
        }

        struct config_vertex_input_state // NOLINTBEGIN
        {
            /*
            typedef struct VkPipelineVertexInputStateCreateInfo {
            VkStructureType                             sType;
            const void*                                 pNext;
            VkPipelineVertexInputStateCreateFlags       flags;
            uint32_t                                    vertexBindingDescriptionCount;
            const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
            uint32_t                                    vertexAttributeDescriptionCount;
            const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
            } VkPipelineVertexInputStateCreateInfo;
            */
            VkPipelineVertexInputStateCreateInfo create() noexcept
            {
                return {.sType = sType<VkPipelineVertexInputStateCreateInfo>(),
                        .pNext = pNext,
                        .flags = flags,
                        .vertexBindingDescriptionCount =
                            static_cast<uint32_t>(vertexBindingDescriptions.size()),
                        .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
                        .vertexAttributeDescriptionCount =
                            static_cast<uint32_t>(vertexAttributeDescriptions.size()),
                        .pVertexAttributeDescriptions =
                            vertexAttributeDescriptions.data()};
            }
            const void *pNext{};
            VkPipelineVertexInputStateCreateFlags flags{};
            std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
            std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
        }; // NOLINTEND
        auto &configVertexInputState(
            config_vertex_input_state vertex_input_state) noexcept
        {
            configVertexInputState_ = std::move(vertex_input_state);
            return *this;
        }
        // VkPipelineInputAssemblyStateCreateInfo
        using config_assembly_state = VkPipelineInputAssemblyStateCreateInfo;
        auto &configAssemblyState(config_assembly_state assembly_state)
        {
            configAssemblyState_ = assembly_state;
            return *this;
        }

        // VkPipelineTessellationStateCreateInfo
        using config_tessellation_state = VkPipelineTessellationStateCreateInfo;
        auto &configTessellationState(config_tessellation_state tessellation_state)
        {
            configTessellationState_ = tessellation_state;
            return *this;
        }

        struct config_viewport_state // NOLINTBEGIN
        {
            /*
            typedef struct VkPipelineViewportStateCreateInfo {
                VkStructureType                       sType;
                const void*                           pNext;
                VkPipelineViewportStateCreateFlags    flags;
                uint32_t                              viewportCount;
                const VkViewport*                     pViewports;
                uint32_t                              scissorCount;
                const VkRect2D*                       pScissors;
            } VkPipelineViewportStateCreateInfo;
            */
            VkPipelineViewportStateCreateInfo create() noexcept
            {
                return {.sType = sType<VkPipelineViewportStateCreateInfo>(),
                        .pNext = pNext,
                        .flags = flags,
                        .viewportCount = static_cast<uint32_t>(viewports.size()),
                        .pViewports = viewports.data(),
                        .scissorCount = static_cast<uint32_t>(scissors.size()),
                        .pScissors = scissors.data()};
            }
            const void *pNext{};
            VkPipelineViewportStateCreateFlags flags{};
            std::vector<VkViewport> viewports;
            std::vector<VkRect2D> scissors;
        }; // NOLINTEND
        auto &configViewportState(config_viewport_state viewport_state) noexcept
        {
            configViewportState_ = std::move(viewport_state);
            return *this;
        }

        using config_rasterization_state = VkPipelineRasterizationStateCreateInfo;

        auto &configRasterizationState(
            config_rasterization_state rasterization_state) noexcept
        {
            configRasterizationState_ = rasterization_state;
            return *this;
        }
        using config_multisample_state = VkPipelineMultisampleStateCreateInfo;
        auto &configMultisampleState(config_multisample_state multisample_state) noexcept
        {
            configMultisampleState_ = multisample_state;
            return *this;
        }
        using config_depth_stencil_state = VkPipelineDepthStencilStateCreateInfo;
        auto &configDepthStencilState(config_depth_stencil_state depth_stencil) noexcept
        {
            configDepthStencilState_ = depth_stencil;
            return *this;
        }

        struct config_color_blend_state // NOLINTBEGIN
        {
            /*
            typedef struct VkPipelineColorBlendStateCreateInfo {
                VkStructureType                               sType;
                const void*                                   pNext;
                VkPipelineColorBlendStateCreateFlags          flags;
                VkBool32                                      logicOpEnable;
                VkLogicOp                                     logicOp;
                uint32_t                                      attachmentCount;
                const VkPipelineColorBlendAttachmentState*    pAttachments;
                float                                         blendConstants[4];
            } VkPipelineColorBlendStateCreateInfo;
            */
            VkPipelineColorBlendStateCreateInfo create() noexcept
            {
                return {.sType = sType<VkPipelineColorBlendStateCreateInfo>(),
                        .pNext = pNext,
                        .flags = static_cast<VkPipelineColorBlendStateCreateFlags>(flags),
                        .logicOpEnable = logicOpEnable,
                        .logicOp = logicOp,
                        .attachmentCount = static_cast<uint32_t>(attachments.size()),
                        .pAttachments = attachments.data(),
                        .blendConstants = {blendConstants[0], blendConstants[1],
                                           blendConstants[2], blendConstants[3]}};
            }
            const void *pNext{};
            VkPipelineColorBlendStateCreateFlagBits flags{};
            VkBool32 logicOpEnable{};
            VkLogicOp logicOp{};
            std::vector<VkPipelineColorBlendAttachmentState> attachments;
            float blendConstants[4]{};
        }; // NOLINTEND

        auto &configColorBlendState(config_color_blend_state color_blend) noexcept
        {
            configColorBlendState_ = std::move(color_blend);
            return *this;
        }

        struct config_dynamic_state // NOLINTBEGIN
        {
            /*
            typedef struct VkPipelineDynamicStateCreateInfo {
                VkStructureType                      sType;
                const void*                          pNext;
                VkPipelineDynamicStateCreateFlags    flags;
                uint32_t                             dynamicStateCount;
                const VkDynamicState*                pDynamicStates;
            } VkPipelineDynamicStateCreateInfo;
            */
            VkPipelineDynamicStateCreateInfo create() noexcept
            {
                return {.sType = sType<VkPipelineDynamicStateCreateInfo>(),
                        .pNext = pNext,
                        .flags = flags,
                        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                        .pDynamicStates = dynamicStates.data()};
            }
            const void *pNext{};
            VkPipelineDynamicStateCreateFlags flags{};
            std::vector<VkDynamicState> dynamicStates;
        }; // NOLINTEND

        auto &configDynamicState(config_dynamic_state dynamic_state) noexcept
        {
            configDynamicState_ = std::move(dynamic_state);
            return *this;
        }

        //--------------set chain-------------------
        // auto &pNext(const void *pNext) noexcept
        // {
        //     createInfo_.pNext = pNext;
        //     return *this;
        // }
        auto &setflags(VkPipelineCreateFlagBits flags) noexcept
        {
            createInfo_.flags = flags;
            return *this;
        }

        auto &setLayout(VkPipelineLayout layout)
        {
            createInfo_.layout = layout;
            return *this;
        }

        auto &setRenderPass(const VkRenderPass &renderPass)
        {
            createInfo_.renderPass = renderPass;
            return *this;
        }
        auto &setSubpass(uint32_t subpass)
        {
            createInfo_.subpass = subpass;
            return *this;
        }
        auto &setBasePipelineHandle(VkPipeline basePipelineHandle)
        {
            createInfo_.basePipelineHandle = basePipelineHandle;
            return *this;
        }
        auto &setBasePipelineIndex(int32_t basePipelineIndex)
        {
            createInfo_.basePipelineIndex = basePipelineIndex;
            return *this;
        }
        auto &setPipelineCache(
            const std::optional<VkPipelineCache> &pipelineCache) noexcept
        {
            pipelineCache_ = pipelineCache;
            return *this;
        }
        //---------------------------------

        [[nodiscard]] VkPipeline create(const void *pNext)
        {
            // c0: pNext
            createInfo_.pNext = pNext;

            // c1: flags

            // c2: stageCount and  pStages
            std::vector<shader_module> shaders;
            std::vector<VkPipelineShaderStageCreateInfo> stages;
            shaders.reserve(configShaderStages_.size());
            stages.reserve(configShaderStages_.size());
            for (auto &config : configShaderStages_)
            {
                shaders.push_back(config.createShaders(logicalDevice_));
                stages.push_back(config.create());

                stages.back().module = shaders.back().raw_data();
            }
            createInfo_.stageCount = stages.size();
            createInfo_.pStages = stages.data();

            // c3: pVertexInputState
            VkPipelineVertexInputStateCreateInfo vertexInputInfo =
                configVertexInputState_.create();
            createInfo_.pVertexInputState = &vertexInputInfo;

            // c4: pInputAssemblyState
            configAssemblyState_.sType = sType<VkPipelineInputAssemblyStateCreateInfo>();
            createInfo_.pInputAssemblyState = &configAssemblyState_;

            // c5: pTessellationState
            configTessellationState_.sType =
                sType<VkPipelineTessellationStateCreateInfo>();
            createInfo_.pTessellationState = &configTessellationState_;

            // c6: pViewportState
            VkPipelineViewportStateCreateInfo viewportState =
                configViewportState_.create();
            createInfo_.pViewportState = &viewportState;

            // c7: pRasterizationState
            configRasterizationState_.sType =
                sType<VkPipelineRasterizationStateCreateInfo>();
            createInfo_.pRasterizationState = &configRasterizationState_;

            // c8: configMultisampleState_
            configMultisampleState_.sType = sType<VkPipelineMultisampleStateCreateInfo>();
            createInfo_.pMultisampleState = &configMultisampleState_;

            // c9: pDepthStencilState
            configDepthStencilState_.sType =
                sType<VkPipelineDepthStencilStateCreateInfo>();
            createInfo_.pDepthStencilState = &configDepthStencilState_;

            // c10: pColorBlendState
            VkPipelineColorBlendStateCreateInfo colorBlendState =
                configColorBlendState_.create();
            createInfo_.pColorBlendState = &colorBlendState;

            // c11: pDynamicState
            VkPipelineDynamicStateCreateInfo dynamicState = configDynamicState_.create();
            createInfo_.pDynamicState = &dynamicState;

            // c12: layout;
            MCS_ASSERT(createInfo_.layout != nullptr);

            // c13: renderPass

            // c14: subpass

            // c15: basePipelineHandle

            // c16: basePipelineIndex

            return logicalDevice_->createGraphicsPipelines(
                pipelineCache_.value_or(nullptr), 1, createInfo_,
                logicalDevice_->allocator());
        }
        template <typename... T>
        [[nodiscard]] VkPipeline create(structure_chain<T...> next)
        {
            return create(&next.head());
        }
        [[nodiscard]] VkPipeline create()
        {
            return create(nullptr);
        }

      private:
        const LogicalDevice *logicalDevice_;

        /*
        typedef struct VkGraphicsPipelineCreateInfo {
            VkStructureType                                  sType;
            const void*                                      pNext;
            VkPipelineCreateFlags                            flags;
            uint32_t                                         stageCount;
            const VkPipelineShaderStageCreateInfo*           pStages;
            const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
            const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
            const VkPipelineTessellationStateCreateInfo*     pTessellationState;
            const VkPipelineViewportStateCreateInfo*         pViewportState;
            const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
            const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
            const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
            const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
            const VkPipelineDynamicStateCreateInfo*          pDynamicState;
            VkPipelineLayout                                 layout;
            VkRenderPass                                     renderPass;
            uint32_t                                         subpass;
            VkPipeline                                       basePipelineHandle;
            int32_t                                          basePipelineIndex;
        } VkGraphicsPipelineCreateInfo;
        */
        VkGraphicsPipelineCreateInfo createInfo_{
            .sType = sType<VkGraphicsPipelineCreateInfo>()};

        std::vector<config_shader_stage> configShaderStages_;
        config_vertex_input_state configVertexInputState_;
        config_assembly_state configAssemblyState_{};
        config_tessellation_state configTessellationState_{};
        config_viewport_state configViewportState_;
        config_rasterization_state configRasterizationState_{};
        config_multisample_state configMultisampleState_{};
        config_depth_stencil_state configDepthStencilState_{};
        config_color_blend_state configColorBlendState_;
        config_dynamic_state configDynamicState_;

        std::optional<VkPipelineCache> pipelineCache_;
    };
} // namespace mcs::vulkan::core