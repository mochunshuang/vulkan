#pragma once

#include "create_image.hpp"
#include "deep_image.hpp"
#include "make_image_base.hpp"
#include "utils/vk_exception.hpp"

#include <algorithm>

namespace mcs::vulkan
{
    template <typename context>
    struct make_deep_image : make_image_base<make_deep_image<context>, context>
    {
        using context_type = context;
        using base = make_image_base<make_deep_image<context>, context>;

        constexpr explicit make_deep_image(context &ctx) noexcept : base{ctx} {}

        using format_properties_type = bool(VkFormatProperties);

        static bool selectDeepFormat(VkFormatProperties props, VkImageTiling tiling,
                                     VkFormatFeatureFlags features) noexcept
        {

            if (tiling == VkImageTiling::VK_IMAGE_TILING_LINEAR &&
                (props.linearTilingFeatures & features) == features)
                return true;

            if (tiling == VkImageTiling::VK_IMAGE_TILING_OPTIMAL &&
                (props.optimalTilingFeatures & features) == features)
                return true;

            return false;
        }

        [[nodiscard]] VkFormat findDepthFormat(
            const std::vector<VkFormat> &candidates) const
        {
            if (requiredFormatProperties_ == nullptr)
                throw utils::make_vk_exception(
                    "requiredFormatProperties() function not set.");

            const physical_device &physicalDevice =
                base::context()->ref_physical_device();
            auto formatIt = std::ranges::find_if(candidates, [&](const VkFormat &format) {
                return (*requiredFormatProperties_)(
                    physicalDevice.getFormatProperties(format));
            });

            if (formatIt == candidates.end())
                throw utils::make_vk_exception("failed to find supported format!");
            return *formatIt;
        }

        [[nodiscard]] const VkExtent2D &ref_surfaceExtent() const noexcept // NOLINT
        {
            return surfaceExtent_;
        }
        auto &setSurfaceExtent(const VkExtent2D &surfaceExtent) noexcept
        {
            surfaceExtent_ = surfaceExtent;
            return *this;
        }

        template <typename Fn>
        auto &requiredFormatProperties(Fn &&requiredFormatPropertiesFn)
        {
            requiredFormatProperties_ = std::forward<Fn>(requiredFormatPropertiesFn);
            return *this;
        }

        deep_image build()
        {
            base::ensureValid();

            VkImageView imageView = nullptr;

            auto *context_ = base::context();
            const physical_device &physicalDevice = context_->ref_physical_device();
            const logical_device &device = context_->ref_logical_device();

            auto &imageCreateInfoFn_ = base::imageCreateInfoFn();
            auto &imageViewCreateInfoFn_ = base::imageViewCreateInfoFn();

            try
            {
                auto imageCreateInfo_ = (*imageCreateInfoFn_)(this);
                auto [image, imageMemory] = create_image(
                    physicalDevice, device, imageCreateInfo_, base::properties());
                auto viewCreateInfo = (*imageViewCreateInfoFn_)(imageCreateInfo_, image);
                imageView = device.createImageView(viewCreateInfo);
                return deep_image{image_base{device, image, imageMemory, imageView},
                                  imageCreateInfo_.format};
            }
            catch (...)
            {
                if (imageView != nullptr)
                    device.destroyImageView(imageView);
                throw;
            }
        }

      private:
        format_properties_type *requiredFormatProperties_{};

        VkExtent2D surfaceExtent_{};
    };

}; // namespace mcs::vulkan