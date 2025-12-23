#include "./head.hpp"

#include <exception>
#include <iostream>
#include <print>
#include <vulkan/vulkan_core.h>

void base()
{
    {
        if (::glfwInit() != GLFW_TRUE)
        {
            const char *description = nullptr;
            int code = ::glfwGetError(&description);
            throw std::runtime_error(
                std::format("[GLFW Error: {}]: {}", code, description));
        }

        // c1: 1. extensions

        auto available_instance_extensions = mcs::vulkan::instance::availableExtension();
        std::vector<const char *> required_instance_extensions{};

        mcs::vulkan::wsi::glfw::Window::addRequiredExtension(
            required_instance_extensions);
        mcs::vulkan::debug_extension::addRequiredExtension(required_instance_extensions);
        if (!mcs::vulkan::instance::checkExtensionSupport(required_instance_extensions,
                                                          available_instance_extensions))
            throw std::runtime_error("Required instance extensions are missing.");

        // c1: 2. layers
        auto supported_instance_layers = mcs::vulkan::instance::availableLayer();
        std::vector<const char *> requested_instance_layers{};
        mcs::vulkan::debug_extension::addRequiredLayer(requested_instance_layers);
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo =
            mcs::vulkan::debug_extension::defaultCreateInfo();
        if (!mcs::vulkan::instance::checkLayerSupport(requested_instance_layers,
                                                      supported_instance_layers))
            throw std::runtime_error("Required instance layer are missing.");

        // NOTE: 必须在创建的时候关联 扩展，才启动扩展
        auto appInfo = mcs::vulkan::make_instance::defaultApplicationInfo();
        VkInstanceCreateInfo createInfo = {
            .sType = mcs::vulkan::sType<VkInstanceCreateInfo>(),
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requested_instance_layers.size()),
            .ppEnabledLayerNames = requested_instance_layers.data(),
            .enabledExtensionCount =
                static_cast<uint32_t>(required_instance_extensions.size()),
            .ppEnabledExtensionNames = required_instance_extensions.data(),
        };
        // set debuger
        createInfo.pNext = &debugCreateInfo; // c1: create with Debug

        VkInstance instance = nullptr;
        VkDebugUtilsMessengerEXT debug_ext = nullptr;

        // c1: 3. CreateInstance
        mcs::vulkan::utils::check_vk_result(
            ::vkCreateInstance(&createInfo, nullptr, &instance));

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {

            if (func(instance, &debugCreateInfo, nullptr, &debug_ext) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to set up debug messenger!");
            }
        }
        else
        {
            throw std::runtime_error(
                "failed to get vkCreateDebugUtilsMessengerEXT function!");
        }

        using surface = mcs::vulkan::wsi::glfw::Window;

        surface window{};
        window.setup({800, 600}, "test");
        window.createVkSurfaceKHR(instance);

        while (window.shouldClose() == 0)
        {
            surface::pollEvents();

            // 游戏逻辑...
            // NOTE: 黑色背景需要vulkan渲染
            if (window.framebufferResized())
            {
                std::cout << "Framebuffer resized!" << '\n';
                window.refFramebufferResized() = false;
            }
        }

        window.teardown();
    }
}

int main()
{

    try
    {
        using surface = mcs::vulkan::wsi::glfw::Window;

        surface window{};
        window.setup({.width = 800, .height = 600}, "test");
        mcs::vulkan::context_base ctx;
        ctx.createInstance(
               mcs::vulkan::make_instance{}
                   .enableDebugExtension()
                   .enableSurfaceExtension<surface>()
                   .checkExtensionSupport()
                   .checkLayerSupport()
                   .build(mcs::vulkan::make_instance::defaultApplicationInfo()))
            .createSurface(window);

        {
            mcs::vulkan::context_base ctx;
        }

        while (window.shouldClose() == 0)
        {
            surface::pollEvents();

            // 游戏逻辑...
            // NOTE: 黑色背景需要vulkan渲染
            if (window.framebufferResized())
            {
                std::cout << "Framebuffer resized!" << '\n';
                window.refFramebufferResized() = false;
            }
        }
        window.teardown();
    }
    catch (std::exception &e)
    {
        std::println("{}", e.what());
    }

    std::cout << "main done\n";
    return 0;
}