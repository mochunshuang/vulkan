
// NOTE: 没有这一行宏，createInstance 失败
#include <utility>
#define GLFW_INCLUDE_VULKAN // REQUIRED only for GLFW CreateWindowSurface.
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

auto get_extensions(vk::raii::Context &context)
{
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
        if (std::ranges::none_of(extensionProperties, [glfwExtension = glfwExtensions[i]](
                                                          auto const &extensionProperty) {
                return strcmp(extensionProperty.extensionName, glfwExtension) == 0;
            }))
        {
            throw std::runtime_error(std::format(
                "Required GLFW extension not supported: {}", glfwExtensions[i]));
        }
    }
    return std::make_pair(glfwExtensionCount, glfwExtensions);
}

struct glfw_context
{
    GLFWwindow *window = nullptr;

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
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
        auto [count, name] = get_extensions(context);
        vk::InstanceCreateInfo createInfo{.pApplicationInfo = &appInfo,
                                          .enabledExtensionCount = count,
                                          .ppEnabledExtensionNames = name};
        instance = vk::raii::Instance(context, createInfo);
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
    glfw_context ctx;

    void initVulkan()
    {
        ctx.createInstance();
    }

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