#pragma once

#include "color_image.hpp"
#include "make_image_base.hpp"

#include "create_image.hpp"

namespace mcs::vulkan
{
    template <typename context>
    struct make_color_image : make_image_base<make_color_image<context>, context>
    {
        using context_type = context;
        using base = make_image_base<make_color_image<context>, context>;

        constexpr explicit make_color_image(context &ctx) noexcept : base{ctx} {}

        [[nodiscard]] const VkExtent2D &ref_surfaceExtent() const noexcept // NOLINT
        {
            return surfaceExtent_;
        }
        auto &setSurfaceExtent(const VkExtent2D &surfaceExtent) noexcept
        {
            surfaceExtent_ = surfaceExtent;
            return *this;
        }
        color_image build()
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
                return color_image{device, image, imageMemory, imageView};
            }
            catch (...)
            {
                if (imageView != nullptr)
                    device.destroyImageView(imageView);
                throw;
            }
        }

        const auto *surfaceImpl() const noexcept
            requires(requires() { base::context()->surfaceImpl(); })
        {
            return base::context()->surfaceImpl();
        }

      private:
        VkExtent2D surfaceExtent_{};
    };

}; // namespace mcs::vulkan
