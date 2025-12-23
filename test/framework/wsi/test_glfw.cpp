#include <iostream>

#include "../head.hpp"

// NOLINTBEGIN

int main()
{
    try
    {
        mcs::vulkan::wsi::glfw::Window window;

        // 方法1：创建时不全屏
        window.setup({800, 600}, "test");

        while (!window.shouldClose())
        {
            window.pollEvents();

            // 游戏逻辑...
            // NOTE: 黑色背景需要vulkan渲染
            if (window.framebufferResized())
            {
                std::cout << "Framebuffer resized!" << std::endl;
                window.refFramebufferResized() = false;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    VkLayerProperties a{};

    return EXIT_SUCCESS;
}
// NOLINTEND