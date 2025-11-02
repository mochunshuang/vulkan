#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

// NOLINTBEGIN
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

class HelloTriangleApplication
{
  public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

  private:
    GLFWwindow *window;
    VkInstance instance;

    void initWindow()
    {
        glfwInit();
        // GLFW 最初设计用于创建 OpenGL 上下文，所以我们需要告诉它不要使用后续调用来创建
        // OpenGL 上下文
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // 处理调整大小的窗口需要特别注意,现在禁用
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        // 第四个参数允许您选择指定一个要打开窗口的监视器，最后一个参数仅与 OpenGL 相关。
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    // NOTE:第一件事是通过创建实例来初始化 Vulkan 库
    void initVulkan()
    {
        createInstance();
    }
    void createInstance()
    {
        VkApplicationInfo appInfo{};
        // NOTE: Vulkan 中的许多结构体都要求您在 sType 成员中显式指定类型
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // NOTE: Vulkan
        // 中的大量信息是通过结构体而不是函数参数传递的，我们将不得不填写另一个结构体，以便为创建实例提供足够的信息
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // NOTE: Vulkan 是一个平台无关的 API，这意味着您需要一个扩展来与窗口系统进行交互。
        // NOTE: GLFW 有一个方便的内置函数，可以返回它需要执行此操作的扩展
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        createInfo.enabledLayerCount = 0;

        // 我们现在已经指定了 Vulkan 创建实例所需的一切，我们终于可以发出 vkCreateInstance
        // 调用了
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }
    }

    // 为了使应用程序一直运行到发生错误或窗口关闭为止
    void mainLoop()
    {
        while (glfwWindowShouldClose(window) == 0)
        {
            glfwPollEvents();
        }
    }

    void cleanup()
    {
        // NOTE: VkInstance 应该在程序退出之前销毁。
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }
};

int main()
{
    HelloTriangleApplication app;

    try
    {
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