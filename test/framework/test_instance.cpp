#include "./head.hpp"

#include <exception>
#include <iostream>
#include <print>
#include <utility>

int main()
{
    // mcs::vulkan::wsi::glfw::Window
    try
    {
        using surface = mcs::vulkan::wsi::glfw::Window;
        auto instance = mcs::vulkan::make_instance{}
                            .enableDebugExtension()
                            .enableSurfaceExtension<surface>()
                            .checkExtensionSupport()
                            .checkLayerSupport()
                            .build(mcs::vulkan::make_instance::defaultApplicationInfo());
        mcs::MCS_ASSERT(instance.valid());
        mcs::MCS_ASSERT(instance.isEnableDebugExtension());

        [[maybe_unused]] auto new_inst = std::move(instance);
        mcs::MCS_ASSERT(not instance.valid());
        mcs::MCS_ASSERT(not instance.isEnableDebugExtension());

        {
            mcs::vulkan::instance ist;
            mcs::MCS_ASSERT(not ist.valid());
            mcs::MCS_ASSERT(not ist.isEnableDebugExtension());
        }
    }
    catch (std::exception &e)
    {
        std::println("{}", e.what());
    }

    std::cout << "main done\n";
    return 0;
}