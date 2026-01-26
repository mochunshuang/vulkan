#pragma once

namespace mcs::vulkan::core
{
    struct resources_interface
    {
        resources_interface() = default;
        resources_interface(const resources_interface &) = default;
        resources_interface(resources_interface &&) = default;
        resources_interface &operator=(const resources_interface &) = default;
        resources_interface &operator=(resources_interface &&) = default;
        constexpr virtual ~resources_interface() = default;
        constexpr virtual void reCreateResources() = 0;
        constexpr virtual void destroyResources() = 0;
    };

}; // namespace mcs::vulkan::core