#pragma once

#include "./descriptor_resource.hpp"
#include <utility>
#include <vector>

namespace mcs::vulkan
{
    template <typename Context_type>
    struct make_descriptor_resource
    {
        using this_type = make_descriptor_resource<Context_type>;

        using descriptor_pool_create_info_type = VkDescriptorPoolCreateInfo(this_type *);
        using descriptor_set_layout_create_info_type =
            VkDescriptorSetLayoutCreateInfo(this_type *);

        constexpr explicit make_descriptor_resource(Context_type &context) noexcept
            : context_{&context}
        {
        }

        [[nodiscard]] const std::vector<VkDescriptorPoolSize> &ref_poolSizes() // NOLINT
            const noexcept
        {
            return poolSizes_;
        }
        [[nodiscard]] auto &ref_layoutBinding() const noexcept // NOLINT
        {
            return layoutBinding_;
        }

        auto &requiredPoolSizes(std::vector<VkDescriptorPoolSize> poolSizes) noexcept
        {
            poolSizes_ = std::move(poolSizes);
            return *this;
        }
        auto &requiredDescriptorPoolCreateInfo(
            descriptor_pool_create_info_type *descriptorPoolCreateInfoFn) noexcept
        {
            descriptorPoolCreateInfoFn_ = descriptorPoolCreateInfoFn;
            return *this;
        }
        auto &requiredLayoutBinding(
            std::vector<VkDescriptorSetLayoutBinding> layoutBinding) noexcept
        {
            layoutBinding_ = std::move(layoutBinding);
            return *this;
        }

        auto &requiredDescriptorSetLayoutCreateInfoFn(
            descriptor_set_layout_create_info_type
                *descriptorSetLayoutCreateInfoFn) noexcept
        {
            descriptorSetLayoutCreateInfoFn_ = descriptorSetLayoutCreateInfoFn;
            return *this;
        }

        descriptor_resource build()
        {
            if (descriptorPoolCreateInfoFn_ == nullptr)
                throw utils::make_vk_exception(
                    "requiredDescriptorPoolCreateInfo function not set.");
            if (descriptorSetLayoutCreateInfoFn_ == nullptr)
                throw utils::make_vk_exception(
                    "requiredDescriptorSetLayoutCreateInfoFn function not set.");

            const auto &device = context_->ref_logical_device();
            VkDescriptorPool descriptorPool = nullptr;
            VkDescriptorSetLayout descriptorSetLayout = nullptr;
            try
            {
                VkDescriptorPoolCreateInfo poolCreateInfo =
                    (*descriptorPoolCreateInfoFn_)(this);
                descriptorPool = device.createDescriptorPool(poolCreateInfo, nullptr);

                VkDescriptorSetLayoutCreateInfo layoutInfo =
                    (*descriptorSetLayoutCreateInfoFn_)(this);
                descriptorSetLayout =
                    device.createDescriptorSetLayout(layoutInfo, nullptr);

                return descriptor_resource{device, descriptorPool, descriptorSetLayout};
            }
            catch (...)
            {
                if (descriptorSetLayout != nullptr)
                    device.destroyDescriptorSetLayout(descriptorSetLayout, nullptr);
                if (descriptorPool != nullptr)
                    device.destroyDescriptorPool(descriptorPool, nullptr);
                throw;
            }
        }

      private:
        Context_type *context_;
        std::vector<VkDescriptorPoolSize> poolSizes_;
        descriptor_pool_create_info_type *descriptorPoolCreateInfoFn_{};
        std::vector<VkDescriptorSetLayoutBinding> layoutBinding_;
        descriptor_set_layout_create_info_type *descriptorSetLayoutCreateInfoFn_{};
    };

}; // namespace mcs::vulkan