#include <iostream>
#include <cassert>
#include <vulkan/vulkan.h>
// NOLINTBEGIN
void testVulkanAggregateInitialization()
{
    std::cout << "=== Vulkan 复杂类型聚合初始化测试 ===\n\n";

    // 1. VkApplicationInfo 部分初始化
    std::cout << "1. VkApplicationInfo 部分初始化:\n";
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan Test App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Test Engine",
        // engineVersion, apiVersion 未指定
    };
    assert(appInfo.sType == VK_STRUCTURE_TYPE_APPLICATION_INFO);
    assert(appInfo.engineVersion == 0); // 未指定，应该为0
    assert(appInfo.apiVersion == 0);    // 未指定，应该为0
    std::cout << "   VkApplicationInfo 部分初始化成功 ✓\n";

    // 2. VkInstanceCreateInfo 复杂部分初始化
    std::cout << "\n2. VkInstanceCreateInfo 复杂部分初始化:\n";
    VkInstanceCreateInfo instanceInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        // ppEnabledLayerNames, enabledExtensionCount, ppEnabledExtensionNames 未指定
    };
    assert(instanceInfo.pApplicationInfo == &appInfo);
    assert(instanceInfo.ppEnabledLayerNames == nullptr);
    assert(instanceInfo.ppEnabledExtensionNames == nullptr);
    std::cout << "   VkInstanceCreateInfo 部分初始化成功 ✓\n";

    // 3. VkDeviceQueueCreateInfo 带数组的部分初始化
    std::cout << "\n3. VkDeviceQueueCreateInfo 带数组的部分初始化:\n";
    float queuePriorities[] = {1.0f, 0.5f};
    VkDeviceQueueCreateInfo queueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = 0,
        .queueCount = 2,
        .pQueuePriorities = queuePriorities,
        // flags 未指定
    };
    assert(queueInfo.queueCount == 2);
    assert(queueInfo.flags == 0); // 未指定，应该为0
    std::cout << "   VkDeviceQueueCreateInfo 部分初始化成功 ✓\n";

    // 4. VkPhysicalDeviceFeatures 复杂位域部分初始化
    std::cout << "\n4. VkPhysicalDeviceFeatures 位域部分初始化:\n";
    VkPhysicalDeviceFeatures features = {
        .geometryShader = VK_TRUE,
        .samplerAnisotropy = VK_TRUE,
        // 其他 50+ 个特性未指定
    };
    assert(features.geometryShader == VK_TRUE);
    assert(features.samplerAnisotropy == VK_TRUE);
    assert(features.tessellationShader == VK_FALSE); // 未指定，应该为 VK_FALSE
    assert(features.multiViewport == VK_FALSE);      // 未指定，应该为 VK_FALSE
    std::cout << "   VkPhysicalDeviceFeatures 部分初始化成功 ✓\n";

    // 5. VkDeviceCreateInfo 多层嵌套部分初始化
    std::cout << "\n5. VkDeviceCreateInfo 多层嵌套部分初始化:\n";
    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .pEnabledFeatures = &features,
        .enabledExtensionCount = 0,
        // ppEnabledExtensionNames, enabledLayerCount, ppEnabledLayerNames 未指定
    };
    assert(deviceInfo.queueCreateInfoCount == 1);
    assert(deviceInfo.ppEnabledExtensionNames == nullptr);
    assert(deviceInfo.enabledLayerCount == 0); // 未指定，应该为0
    std::cout << "   VkDeviceCreateInfo 多层嵌套部分初始化成功 ✓\n";

    // 6. VkImageCreateInfo 复杂配置部分初始化
    std::cout << "\n6. VkImageCreateInfo 复杂配置部分初始化:\n";
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {.width = 1024, .height = 768, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        // flags, initialLayout, queueFamilyIndexCount, pQueueFamilyIndices 未指定
    };
    assert(imageInfo.extent.width == 1024);
    assert(imageInfo.flags == 0);                                 // 未指定，应该为0
    assert(imageInfo.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED); // 未指定，默认值
    assert(imageInfo.queueFamilyIndexCount == 0);                 // 未指定，应该为0
    std::cout << "   VkImageCreateInfo 复杂配置部分初始化成功 ✓\n";

    // 7. VkRenderPassCreateInfo 带子依赖的部分初始化
    std::cout << "\n7. VkRenderPassCreateInfo 带子依赖的部分初始化:\n";
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        // srcAccessMask, dependencyFlags 未指定
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .dependencyCount = 1,
        .pDependencies = &dependency,
        // attachmentCount, pAttachments, subpassCount, pSubpasses 未指定
    };
    assert(renderPassInfo.dependencyCount == 1);
    assert(renderPassInfo.attachmentCount == 0); // 未指定，应该为0
    assert(dependency.srcAccessMask == 0);       // 未指定，应该为0
    std::cout << "   VkRenderPassCreateInfo 带子依赖部分初始化成功 ✓\n";

    // 8. VkGraphicsPipelineCreateInfo 极端复杂部分初始化
    std::cout << "\n8. VkGraphicsPipelineCreateInfo 极端复杂部分初始化:\n";
    VkPipelineShaderStageCreateInfo shaderStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .pName = "main",
        // module, pSpecializationInfo 未指定
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 1,
        .pStages = &shaderStage,
        // layout, renderPass, subpass 等其他 10+ 字段未指定
    };
    assert(pipelineInfo.stageCount == 1);
    assert(pipelineInfo.layout == VK_NULL_HANDLE);     // 未指定，应该为 null
    assert(pipelineInfo.renderPass == VK_NULL_HANDLE); // 未指定，应该为 null
    assert(shaderStage.module == VK_NULL_HANDLE);      // 未指定，应该为 null
    std::cout << "   VkGraphicsPipelineCreateInfo 极端复杂部分初始化成功 ✓\n";

    // 9. VkSubmitInfo 命令缓冲区部分初始化
    std::cout << "\n9. VkSubmitInfo 命令缓冲区部分初始化:\n";
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 0,
        // pCommandBuffers, waitSemaphoreCount, pWaitSemaphores, pWaitDstStageMask,
        // signalSemaphoreCount, pSignalSemaphores 未指定
    };
    assert(submitInfo.commandBufferCount == 0);
    assert(submitInfo.pCommandBuffers == nullptr);
    assert(submitInfo.waitSemaphoreCount == 0); // 未指定，应该为0
    std::cout << "   VkSubmitInfo 命令缓冲区部分初始化成功 ✓\n";

    // 10. VkSwapchainCreateInfoKHR 交换链复杂部分初始化
    std::cout << "\n10. VkSwapchainCreateInfoKHR 交换链复杂部分初始化:\n";
    VkSwapchainCreateInfoKHR swapchainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = VK_NULL_HANDLE,
        .minImageCount = 3,
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {.width = 800, .height = 600},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        // oldSwapchain, queueFamilyIndexCount, pQueueFamilyIndices 未指定
    };
    assert(swapchainInfo.minImageCount == 3);
    assert(swapchainInfo.oldSwapchain == VK_NULL_HANDLE); // 未指定，应该为 null
    assert(swapchainInfo.queueFamilyIndexCount == 0);     // 未指定，应该为0
    std::cout << "   VkSwapchainCreateInfoKHR 交换链复杂部分初始化成功 ✓\n";

    // 11. 深度嵌套结构体测试
    std::cout << "\n11. 深度嵌套结构体部分初始化:\n";
    VkRenderPassBeginInfo renderPassBegin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = VK_NULL_HANDLE,
        .framebuffer = VK_NULL_HANDLE,
        .renderArea = {.offset = {.x = 0, .y = 0},
                       .extent = {.width = 1920, .height = 1080}},
        .clearValueCount = 2,
        .pClearValues =
            new VkClearValue[2]{
                {.color = {.float32 = {0.1f, 0.2f, 0.3f, 1.0f}}},
                {.depthStencil = {.depth = 1.0f}} // stencil 未指定
            }
        // pNext 未指定
    };
    assert(renderPassBegin.renderArea.offset.x == 0);
    assert(renderPassBegin.pNext == nullptr);
    assert(renderPassBegin.pClearValues[1].depthStencil.stencil == 0); // 未指定
    std::cout << "   VkRenderPassBeginInfo 深度嵌套部分初始化成功 ✓\n";
    delete[] renderPassBegin.pClearValues;

    // 12. 多层嵌套的管线状态创建信息
    std::cout << "\n12. 多层嵌套的管线状态部分初始化:\n";
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        // vertexBindingDescriptionCount, pVertexBindingDescriptions,
        // vertexAttributeDescriptionCount, pVertexAttributeDescriptions 未指定
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        // primitiveRestartEnable 未指定
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
        // pViewports, pScissors 未指定
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
        // depthClampEnable, rasterizerDiscardEnable, depthBiasEnable 等未指定
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        // sampleShadingEnable, minSampleShading 等未指定
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        // blendEnable, srcColorBlendFactor 等混合参数未指定
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        // logicOpEnable, blendConstants 未指定
    };

    // 验证嵌套结构体的默认值
    assert(vertexInputInfo.vertexBindingDescriptionCount == 0);
    assert(inputAssembly.primitiveRestartEnable == VK_FALSE);
    assert(viewportState.pViewports == nullptr);
    assert(rasterizer.depthClampEnable == VK_FALSE);
    assert(multisampling.sampleShadingEnable == VK_FALSE);
    assert(colorBlendAttachment.blendEnable == VK_FALSE);
    assert(colorBlending.logicOpEnable == VK_FALSE);
    std::cout << "   多层嵌套管线状态部分初始化成功 ✓\n";

    // 13. 复杂描述符集布局嵌套
    std::cout << "\n13. 复杂描述符集布局嵌套部分初始化:\n";
    VkDescriptorSetLayoutBinding layoutBindings[2] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            // pImmutableSamplers 未指定
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            // descriptorCount, pImmutableSamplers 未指定
        }};

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = layoutBindings,
        // flags, pNext 未指定
    };

    assert(layoutBindings[0].pImmutableSamplers == nullptr);
    assert(layoutBindings[1].descriptorCount == 0); // 未指定
    assert(layoutBindings[1].pImmutableSamplers == nullptr);
    assert(layoutInfo.flags == 0);
    std::cout << "   复杂描述符集布局嵌套部分初始化成功 ✓\n";

    // 14. 图像内存屏障深度嵌套
    std::cout << "\n14. 图像内存屏障深度嵌套部分初始化:\n";
    VkImageMemoryBarrier imageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = VK_NULL_HANDLE,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                // baseArrayLayer, layerCount 未指定
            },
        // srcQueueFamilyIndex, dstQueueFamilyIndex 未指定
    };

    VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = nullptr,
        // dependencyFlags, memoryBarrierCount, pMemoryBarriers,
        // bufferMemoryBarrierCount, pBufferMemoryBarriers 未指定
    };

    assert(imageBarrier.subresourceRange.baseArrayLayer == 0); // 未指定
    assert(imageBarrier.subresourceRange.layerCount == 0);     // 未指定
    assert(imageBarrier.srcQueueFamilyIndex == 0);             // 未指定
    assert(dependencyInfo.dependencyFlags == 0);               // 未指定
    assert(dependencyInfo.memoryBarrierCount == 0);            // 未指定
    std::cout << "   图像内存屏障深度嵌套部分初始化成功 ✓\n";

    // 15. 复杂加速结构构建信息嵌套
    std::cout << "\n15. 复杂加速结构构建信息嵌套部分初始化:\n";
    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry =
            {.triangles =
                 {
                     .sType =
                         VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                     .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                     .vertexData = {.deviceAddress = 0},
                     .vertexStride = 12,
                     // maxVertex, indexType, indexData, transformData 未指定
                 }},
        // flags 未指定
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry,
        // srcAccelerationStructure, dstAccelerationStructure,
        // scratchData 等未指定
    };

    assert(geometry.geometry.triangles.maxVertex == 0);           // 未指定
    assert(geometry.geometry.triangles.indexType == 0);           // 未指定，默认值
    assert(geometry.flags == 0);                                  // 未指定
    assert(buildInfo.srcAccelerationStructure == VK_NULL_HANDLE); // 未指定
    std::cout << "   复杂加速结构构建信息嵌套部分初始化成功 ✓\n";
}

int main()
{
    testVulkanAggregateInitialization();
    std::cout << "\n=== 所有 Vulkan 聚合初始化测试通过! ===\n";
    return 0;
}
// NOLINTEND