
#include <GLFW/glfw3.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

import std;
import std.compat;

// NOLINTBEGIN

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

struct context
{
    GLFWwindow *window = nullptr;

    // NOTE: 搭建基本窗口
    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }
    void cleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();
    }
};

class HelloTriangleApplication
{
  public:
    void run()
    {
        ctx.initWindow();
        initVulkan();
        mainLoop();
        ctx.cleanup();
    }

  private:
    context ctx;

    void initVulkan() {}

    void mainLoop()
    {
        while (!glfwWindowShouldClose(ctx.window))
        {
            glfwPollEvents();
        }
    }
};

int main()
{
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
// NOLINTEND