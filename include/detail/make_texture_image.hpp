#pragma once

#include "create_buffer.hpp"
#include "create_image.hpp"
#include "logical_device.hpp"
#include "sType.hpp"
#include "texture_image.hpp"
#include "make_image_base.hpp"
#include "utils/mcs_assert.hpp"
#include "utils/vk_exception.hpp"

#include "copy_raw_data_to_staging_buffer.hpp"

#include "staging_buffer.hpp"

#include "begin_single_time_commands.hpp"
#include "end_single_time_commands.hpp"
#include "copy_buffer_to_image.hpp"

#include "generate_mipmaps.hpp"
#include <utility>

namespace mcs::vulkan
{
    template <typename context>
    struct make_texture_image : make_image_base<make_texture_image<context>, context>
    {
        using base = make_image_base<make_texture_image, context>;
        using base::base;

        using sampler_create_info_type = VkSamplerCreateInfo(make_texture_image *self);

      private:
        auto getRawImage()
        {
            raw_stbi_image raw_image{};
            if (not filePath_.empty())
            {
                try
                {
                    if (flip_)
                        stbi_set_flip_vertically_on_load(1);
                    raw_image = raw_stbi_image{filePath_.data()};
                    if (flip_)
                        stbi_set_flip_vertically_on_load(0);
                    return raw_image;
                }
                catch (...)
                {
                    if (flip_)
                        stbi_set_flip_vertically_on_load(0);
                    throw;
                }
            }
            return raw_image;
        }

      public:
        [[nodiscard]] auto &rawImage() const noexcept
        {
            return rawImage_;
        }

        auto &setFilePath(std::string filePath) noexcept
        {
            filePath_ = std::move(filePath);
            return *this;
        }
        auto &setFlip(bool flip) noexcept
        {
            flip_ = flip;
            return *this;
        }

        template <typename Fn>
        constexpr auto &requiredSamplerCreateInfo(Fn &&samplerCreateInfoFn) noexcept
        {
            samplerCreateInfoFn_ = std::forward<Fn>(samplerCreateInfoFn);
            return *this;
        }

        texture_image build()
        {
            base::ensureValid();

            if (samplerCreateInfoFn_ == nullptr)
                throw utils::make_vk_exception(
                    "requiredSamplerCreateInfo() function not set.");

            rawImage_ = getRawImage();

            if (rawImage_.valid())
            {
                auto *context_ = base::context();
                const physical_device &physicalDevice = context_->ref_physical_device();
                const logical_device &device = context_->ref_logical_device();
                auto *queue = context_->defaultQueue();
                auto *commandPool = context_->defalutCommandPool();

                auto &imageCreateInfoFn_ = base::imageCreateInfoFn();
                auto &imageViewCreateInfoFn_ = base::imageViewCreateInfoFn();

                const auto WIDTH = rawImage_.width();
                const auto HEIGHT = rawImage_.height();
                const auto MIP_LEVELS = rawImage_.mipLevels();
                const auto IMAGE_SIZE = rawImage_.imageSize();

                MCS_ASSERT(IMAGE_SIZE != 0);

                VkImageView imageView = nullptr;
                VkSampler sampler = nullptr;
                try
                {
                    // 0. load texture_img

                    VkBufferCreateInfo bufferInfo{
                        .sType = sType<VkBufferCreateInfo>(),
                        .size = static_cast<VkDeviceSize>(IMAGE_SIZE),
                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
                    staging_buffer stagingBuffer = staging_buffer{
                        create_buffer(physicalDevice, device, bufferInfo,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
                    copy_raw_data_to_staging_buffer(rawImage_, stagingBuffer);

                    // 1. createImage
                    auto imageCreateInfo_ = (*imageCreateInfoFn_)(this);
                    auto [image, imageMemory] = create_image(
                        physicalDevice, device, imageCreateInfo_, base::properties());

                    // 1.1 transitionImageLayout
                    transitionImageLayout(
                        device, queue, commandPool, image, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MIP_LEVELS);

                    copy_buffer_to_image(
                        device, queue, commandPool, image, stagingBuffer.buffer(),
                        static_cast<uint32_t>(WIDTH), static_cast<uint32_t>(HEIGHT));

                    generate_mipmaps(
                        physicalDevice, device, commandPool, queue, image,
                        VkFormat::VK_FORMAT_R8G8B8A8_SRGB,
                        {.width = WIDTH, .height = HEIGHT, .mip_levels = MIP_LEVELS});

                    // 2. createImageView
                    auto viewCreateInfo =
                        (*imageViewCreateInfoFn_)(imageCreateInfo_, image);
                    imageView = device.createImageView(viewCreateInfo);

                    // 3. createTextureSampler
                    auto samplerCreateInfo = (*samplerCreateInfoFn_)(this);

                    sampler = device.createSampler(samplerCreateInfo);

                    return texture_image{
                        image_base{device, image, imageMemory, imageView}, sampler};
                }
                catch (...)
                {
                    if (sampler != nullptr)
                        device.destroySampler(sampler);
                    if (imageView != nullptr)
                        device.destroyImageView(imageView);
                    throw;
                }
            }
            throw utils::make_vk_exception("TODO: when filePath_.empty()");
        }

        // NOTE: 自动生成生成更好。不要私有化默认构造函数
        //  make_texture_image(const make_texture_image &) = delete;
        //  make_texture_image &operator=(const make_texture_image &) = delete;
        //  make_texture_image(make_texture_image &&o) noexcept
        //      : filePath_{std::exchange(o.filePath_, {})},
        //        flip_{std::exchange(o.flip_, {})}, rawImage_{std::move(o.rawImage_)},
        //        samplerCreateInfoFn_{std::exchange(o.samplerCreateInfoFn_, {})} {

        //       };

        // make_texture_image &operator=(make_texture_image &&o) noexcept
        // {
        //     if (&o != this)
        //     {
        //         filePath_ = std::exchange(o.filePath_, {});
        //         flip_ = std::exchange(o.flip_, {});
        //         rawImage_ = std::move(o.rawImage_);
        //         samplerCreateInfoFn_ = std::exchange(o.samplerCreateInfoFn_, {});
        //     }
        //     return *this;
        // }
        // ~make_texture_image()
        // {
        //     filePath_ = {};
        //     flip_ = {};
        //     rawImage_ = {};
        //     samplerCreateInfoFn_ = {};
        // }

      private:
        std::string filePath_;
        bool flip_{};
        raw_stbi_image rawImage_;
        sampler_create_info_type *samplerCreateInfoFn_{};

        constexpr static void transitionImageLayout(
            const logical_device &device, VkQueue queue, VkCommandPool commandPool,
            VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
            uint32_t levelCount)
        {
            auto command = begin_single_time_commands(device, commandPool);

            VkImageMemoryBarrier barrier{
                .sType = sType<VkImageMemoryBarrier>(),
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = levelCount,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};

            VkPipelineStageFlags sourceStage;      // NOLINT
            VkPipelineStageFlags destinationStage; // NOLINT

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                barrier.srcAccessMask = {};
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                     newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else
                throw std::invalid_argument("unsupported layout transition!");

            ::vkCmdPipelineBarrier(command.raw_data(), sourceStage, destinationStage, 0,
                                   0, nullptr, 0, nullptr, 1, &barrier);
            end_single_time_commands(queue, std::move(command));
        }
    };

}; // namespace mcs::vulkan