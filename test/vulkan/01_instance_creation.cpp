import std;
import std.compat;

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

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
    GLFWwindow *window = nullptr;

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;

    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        createInstance();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    void cleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();
    }

    // NOTE: 第一件事是通过创建 一个实例
    void createInstance()
    {
        // NOTE: 配置 我们应用程序的信息
        constexpr vk::ApplicationInfo appInfo{.pApplicationName = "Hello Triangle",
                                              .applicationVersion =
                                                  VK_MAKE_VERSION(1, 0, 0),
                                              .pEngineName = "No Engine",
                                              .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                              .apiVersion = vk::ApiVersion14};

        // Get the required instance extensions from GLFW.
        // NOTE: 查找 GLFW 需要那些扩展属性必须提供
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // Check if the required GLFW extensions are supported by the Vulkan
        // implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (uint32_t i = 0; i < glfwExtensionCount; ++i)
        {
            // NOTE: 有一个不满足抛异常
            if (std::ranges::none_of(
                    extensionProperties,
                    [glfwExtension = glfwExtensions[i]](auto const &extensionProperty) {
                        return strcmp(extensionProperty.extensionName, glfwExtension) ==
                               0;
                    }))
            {
                throw std::runtime_error(std::format(
                    "Required GLFW extension not supported: {}", glfwExtensions[i]));
            }
        }

        vk::InstanceCreateInfo createInfo{.pApplicationInfo = &appInfo,
                                          .enabledExtensionCount = glfwExtensionCount,
                                          .ppEnabledExtensionNames = glfwExtensions};
        instance = vk::raii::Instance(context, createInfo);
    }
};

int main()
{
    try
    {
        // NOTE: 实例的创建
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