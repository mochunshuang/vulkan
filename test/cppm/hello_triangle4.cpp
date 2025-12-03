

#include <cstddef>
#include <exception>
#include <iostream>
#include <type_traits>
#include <utility>

#define GLFW_INCLUDE_VULKAN // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <cassert>

#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_profiles.hpp>
#include <print>
#include <fstream>
#include <string>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

namespace glfw
{
    class Window
    {
      public:
        using window_type = ::GLFWwindow;
        using window_pointer = window_type *;

        void setup(int width, int height, const char *title, ::GLFWmonitor *monitor,
                   ::GLFWwindow *share) noexcept
        {
            glfwLibInit();

            window_ = ::glfwCreateWindow(width, height, title, monitor, share);

            // set context value
            ::glfwSetWindowUserPointer(window_, this);
            ::glfwSetFramebufferSizeCallback(window_, &framebufferResizeCallback);

            // In main function, register callbacks:
            glfwSetKeyCallback(window_, keyCallback);
            glfwSetCursorPosCallback(window_, cursorPositionCallback);
            glfwSetMouseButtonCallback(window_, mouseButtonCallback);
        }
        void teardown() noexcept
        {
            ::glfwDestroyWindow(window_);
            ::glfwTerminate();
        }

        int shouldClose() noexcept
        {
            return ::glfwWindowShouldClose(window_);
        }
        static void pollEvents() noexcept
        {
            ::glfwPollEvents();
        }
        void waitGoodFramebufferSize()
        {
            int width, height; // NOLINT
            ::glfwGetFramebufferSize(window_, &width, &height);
            while (width == 0 || height == 0)
            {
                ::glfwGetFramebufferSize(window_, &width, &height);
                ::glfwWaitEvents();
            }
        }

        [[nodiscard]] window_pointer data() const noexcept
        {
            return window_;
        }
        [[nodiscard]] auto getFramebufferSize() const noexcept
        {
            int width, height; // NOLINT
            glfwGetFramebufferSize(window_, &width, &height);
            return std::make_pair(width, height);
        }

        [[nodiscard]] bool framebufferResized() const noexcept
        {
            return framebufferResized_;
        }
        [[nodiscard]] auto &refFramebufferResized() noexcept
        {
            return framebufferResized_;
        }

        [[nodiscard]] static std::vector<const char *> getRequiredSurfaceExtensions()
        {
            uint32_t glfw_extension_count{0};
            const char **names =
                ::glfwGetRequiredInstanceExtensions(&glfw_extension_count);
            return {names, names + glfw_extension_count};
        }

        constexpr void createVkSurfaceKHR(VkInstance &instance,
                                          VkSurfaceKHR &surface) const
        {
            if (::glfwCreateWindowSurface(instance, data(), nullptr, &surface) != 0)
                throw std::runtime_error("failed to create window surface!");
        }

      private:
        window_pointer window_ = nullptr;
        bool framebufferResized_{};

        static void glfwLibInit() noexcept
        {
            ::glfwInit();
            ::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            ::glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        }
        static void framebufferResizeCallback(GLFWwindow *window, int /*width*/,
                                              int /*height*/) noexcept
        {
            auto *app = static_cast<Window *>(::glfwGetWindowUserPointer(window));
            app->framebufferResized_ = true;
        }

        // NOLINTBEGIN
        // Callback for keyboard input
        static void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                                int mods)
        {
            // 1. 处理 ESC 键：退出程序
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            {
                std::cout << "KEY GLFW_KEY_ESCAPE: PRESS" << '\n';
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            // 2. 新增：处理空格键
            if (key == GLFW_KEY_SPACE)
            {
                if (action == GLFW_PRESS)
                {
                    std::cout << "KEY SPACE: PRESSED" << '\n';
                    // 在这里添加空格键按下的逻辑，例如：
                    // - 切换相机状态
                    // - 暂停/继续游戏
                    // - 触发角色跳跃
                }
                else if (action == GLFW_RELEASE)
                {
                    std::cout << "KEY SPACE: RELEASED" << '\n';
                    // 在这里添加空格键释放的逻辑
                }
                // 注意：GLFW_REPEAT 事件需要启用按键重复（默认禁用）
                else if (action == GLFW_REPEAT)
                {
                    std::cout << "KEY SPACE: REPEATING" << '\n';
                }
            }

            // 3. 你可以继续添加其他按键的判断
            if (key == GLFW_KEY_W)
            {
                // 处理W键（常用于前进）
            }
        }

        // Callback for mouse movement
        static void cursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
        {
            // Handle mouse movement
            std::cout << "Mouse position: " << xpos << ", " << ypos << '\n';
        }

        // Callback for mouse buttons
        static void mouseButtonCallback(GLFWwindow *window, int button, int action,
                                        int mods)
        {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
            {
                // Handle left mouse button press
                std::cout << "Left mouse button pressed" << '\n';
            }
        }
        // NOLINTEND
    };

}; // namespace glfw

// NOLINTBEGIN
template <typename... Args>
static constexpr auto LOGI(std::format_string<Args...> fmt, Args &&...args)
{
    std::println("INFO: {}", std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
static constexpr auto LOGW(std::format_string<Args...> fmt, Args &&...args)
{
    std::println("WARN: {}", std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
static constexpr auto LOGE(std::format_string<Args...> fmt, Args &&...args)
{
    std::println("ERROR: {}", std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
static constexpr auto LOGD(std::format_string<Args...> fmt, Args &&...args)
{
    std::println("DEBUG: {}", std::format(fmt, std::forward<Args>(args)...));
}
// NOLINTEND

struct vulkan
{
    template <::std::size_t, typename T>
    struct product_type_element
    {
        T value; // NOLINT
        auto operator==(const product_type_element &) const -> bool = default;
    };

    template <typename, typename...>
    struct product_type_base;

    template <::std::size_t... I, typename... T>
    struct product_type_base<::std::index_sequence<I...>, T...>
        : protected product_type_element<I, T>...
    {
        constexpr product_type_base(T &&...t) noexcept // NOLINT
            : product_type_element<I, T>{std::move(t)}...
        {
            setupNextChain();
        }

        static consteval ::std::size_t size() noexcept
        {
            return sizeof...(T);
        }

        auto &head() noexcept
        {
            return get<0>();
        }

        template <::std::size_t J>
        constexpr auto get() & noexcept -> decltype(auto)
        {
            return this->element_get<J>(*this);
        }
        template <::std::size_t J>
        constexpr auto get() && noexcept -> decltype(auto)
        {
            return this->element_get<J>(::std::move(*this));
        }
        template <::std::size_t J>
        [[nodiscard]] constexpr auto get() const & noexcept -> decltype(auto)
        {
            return this->element_get<J>(*this);
        }

        auto operator==(const product_type_base &) const -> bool = default;

      private:
        template <::std::size_t J, typename S>
        constexpr static auto element_get( // NOLINT
            product_type_element<J, S> &self) noexcept -> S &
        {
            return self.value;
        }
        template <::std::size_t J, typename S>
        constexpr static auto element_get( // NOLINTNEXTLINE // NOLINT
            product_type_element<J, S> &&self) noexcept -> S &&
        {
            return ::std::move(self.value);
        }
        template <::std::size_t J, typename S>
        constexpr static auto element_get( // NOLINT
            const product_type_element<J, S> &self) noexcept -> const S &
        {
            return self.value;
        }

        constexpr void setupNextChain() noexcept
        {
            ((get<I>().sType = sType<decltype(auto(get<I>()))>()), ...);
            if constexpr (sizeof...(T) > 1)
            {
                []<std::size_t... Ix>(auto *self,
                                      std::index_sequence<Ix...>) constexpr noexcept {
                    ((self->template get<Ix>().pNext = &self->template get<Ix + 1>()),
                     ...);
                }(this, std::make_index_sequence<sizeof...(T) - 1>{});
            }
        }
    };

    template <typename... T>
        requires(sizeof...(T) > 0 && (requires(T &t) { t.pNext = nullptr; } && ...))
    struct structure_chain : product_type_base<::std::index_sequence_for<T...>, T...>
    {
        using product_type_base<::std::index_sequence_for<T...>, T...>::product_type_base;
    };
    template <typename... T>
    structure_chain(T &&...) -> structure_chain<::std::decay_t<T>...>;

    /// @brief  test the result of Vulkan calls which can return an error.
    static constexpr void checkVkResult(const VkResult &err)
    {
        if (err != VkResult::VK_SUCCESS)
            throw std::runtime_error("Detected Vulkan error: ");
    }

    template <typename T>
    static consteval auto sType() -> VkStructureType // NOLINT
    {
        if constexpr (std::is_same_v<T, VkApplicationInfo>)
            return VK_STRUCTURE_TYPE_APPLICATION_INFO;
        else if constexpr (std::is_same_v<T, VkInstanceCreateInfo>)
            return VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkDeviceCreateInfo>)
            return VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkDeviceQueueCreateInfo>)
            return VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkSwapchainCreateInfoKHR>)
            return VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        else if constexpr (std::is_same_v<T, VkImageCreateInfo>)
            return VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkImageViewCreateInfo>)
            return VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkBufferCreateInfo>)
            return VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkMemoryAllocateInfo>)
            return VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        else if constexpr (std::is_same_v<T, VkCommandBufferAllocateInfo>)
            return VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        else if constexpr (std::is_same_v<T, VkCommandPoolCreateInfo>)
            return VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkRenderPassCreateInfo>)
            return VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkFramebufferCreateInfo>)
            return VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineLayoutCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkGraphicsPipelineCreateInfo>)
            return VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkShaderModuleCreateInfo>)
            return VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkSemaphoreCreateInfo>)
            return VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkFenceCreateInfo>)
            return VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkSubmitInfo>)
            return VK_STRUCTURE_TYPE_SUBMIT_INFO;
        else if constexpr (std::is_same_v<T, VkPresentInfoKHR>)
            return VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        else if constexpr (std::is_same_v<T, VkDebugUtilsMessengerCreateInfoEXT>)
            return VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        else if constexpr (std::is_same_v<T, VkPhysicalDeviceFeatures2>)
            return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        else if constexpr (std::is_same_v<T, VkSamplerCreateInfo>)
            return VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkDescriptorSetLayoutCreateInfo>)
            return VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineCacheCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        else if (std::is_same_v<VkPhysicalDeviceVulkan11Features, T>)
            return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        else if constexpr (std::is_same_v<T, VkPhysicalDeviceVulkan13Features>)
            return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        else if constexpr (std::is_same_v<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT,
                                          T>)
            return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
        else if constexpr (std::is_same_v<T, VkPipelineShaderStageCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineVertexInputStateCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineInputAssemblyStateCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineViewportStateCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineRasterizationStateCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineMultisampleStateCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineColorBlendStateCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineDynamicStateCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkPipelineRenderingCreateInfo>)
            return VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        else if constexpr (std::is_same_v<T, VkCommandBufferBeginInfo>)
            return VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        else if constexpr (std::is_same_v<T, VkRenderingInfo>)
            return VK_STRUCTURE_TYPE_RENDERING_INFO;
        else if constexpr (std::is_same_v<T, VkRenderingAttachmentInfo>)
            return VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        else if constexpr (std::is_same_v<T, VkImageMemoryBarrier>)
            return VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        else
            // static_assert(false, "Unknown Vulkan structure type");
            std::terminate();
    }

    struct vulkan_context
    {
        using window_type = glfw::Window;
        // NOLINTNEXTLINE
        static constexpr auto VK_DEBUG_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

        // NOLINTBEGIN
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queueIndex = ~0;
        VkQueue queue = VK_NULL_HANDLE;

        VkSampleCountFlagBits msaaSamples{};
        struct AppInfo // Application info to store profile support
        {
            bool profileSupported = false;
            VpProfileProperties profile;
        } appInfo = {};

        [[nodiscard]] static constexpr auto availableInstanceExtension()
            -> std::vector<VkExtensionProperties>
        {
            // NOTE: Properties 只读的意思
            uint32_t instance_extension_count; // NOLINT
            checkVkResult(::vkEnumerateInstanceExtensionProperties(
                nullptr, &instance_extension_count, nullptr));

            std::vector<VkExtensionProperties> available_instance_extensions(
                instance_extension_count);
            checkVkResult(::vkEnumerateInstanceExtensionProperties(
                nullptr, &instance_extension_count,
                available_instance_extensions.data()));
            return available_instance_extensions;
        }

        static void printAvailableInstanceExtension()
        {
            std::print("AvailableInstanceExtension: [ ");
            for (int i = 0; const auto &e : availableInstanceExtension())
            {
                if (i == 0)
                {
                    std::print("{}", e.extensionName);
                    ++i;
                }
                else
                    std::print(", {}", e.extensionName);
            }
            std::print(" ].\n");
        }

        static void addSurfaceExtension(
            std::vector<const char *> &required_instance_extensions)
        {
            required_instance_extensions.append_range(
                window_type::getRequiredSurfaceExtensions());
        }

        static void addDebugExtension(
            std::vector<const char *> &required_instance_extensions)
        {
            required_instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        [[nodiscard]] static bool checkExtensionSupport(
            const std::vector<const char *> &required,
            const std::vector<VkExtensionProperties> &available =
                availableInstanceExtension())
        {
            for (const auto *extension_name : required)
            {
                bool found = std::ranges::any_of(
                    available, [&](auto const &available_extension) noexcept {
                        return ::strcmp(available_extension.extensionName,
                                        extension_name) == 0;
                    });
                if (!found)
                {
                    // Output an error message for the missing extension
                    LOGE("Required extension not found: {}", extension_name);
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] static constexpr auto availableInstanceLayer()
            -> std::vector<VkLayerProperties>
        {
            uint32_t instance_layer_count; // NOLINT
            checkVkResult(
                ::vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr));

            std::vector<VkLayerProperties> supported_instance_layers(
                instance_layer_count);
            checkVkResult(::vkEnumerateInstanceLayerProperties(
                &instance_layer_count, supported_instance_layers.data()));
            return supported_instance_layers;
        }
        [[nodiscard]] static bool addDebuglayerIfAvailable(
            std::vector<const char *> &requested_instance_layers,
            const std::vector<VkLayerProperties> &supported_instance_layers =
                availableInstanceLayer())
        {
            if (std::ranges::any_of(
                    supported_instance_layers, [](auto const &lp) noexcept {
                        return ::strcmp(lp.layerName, VK_DEBUG_LAYER_NAME) == 0;
                    }))
            {
                requested_instance_layers.push_back(VK_DEBUG_LAYER_NAME);
                LOGI("Enabled Validation Layer {}", VK_DEBUG_LAYER_NAME);
                return true;
            }
            LOGW("Validation Layer {} is not available", VK_DEBUG_LAYER_NAME);
            return false;
        }
        /// @brief A debug callback called from Vulkan validation layers.
        static VKAPI_ATTR VkBool32 VKAPI_CALL
        debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                      VkDebugUtilsMessageTypeFlagsEXT /*message_types*/,
                      const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                      void * /*user_data*/)
        {

            if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
            {
                LOGE("{} Validation Layer: Error: {}: {}", callback_data->messageIdNumber,
                     callback_data->pMessageIdName, callback_data->pMessage);
            }
            else if ((message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
            {
                LOGW("{} Validation Layer: Warning: {}: {}",
                     callback_data->messageIdNumber, callback_data->pMessageIdName,
                     callback_data->pMessage);
            }
            else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) !=
                     0)
            {
                LOGI("{} Validation Layer: Information: {}: {}",
                     callback_data->messageIdNumber, callback_data->pMessageIdName,
                     callback_data->pMessage);
            }
            else if ((message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0)
            {
                LOGD("{} Validation Layer: Verbose: {}: {}",
                     callback_data->messageIdNumber, callback_data->pMessageIdName,
                     callback_data->pMessage);
            }
            return VK_FALSE;
        }

        static constexpr void applyDebugCreateInfo(
            VkDebugUtilsMessengerCreateInfoEXT &debugCreateInfo)
        {
            debugCreateInfo.sType = vulkan::sType<VkDebugUtilsMessengerCreateInfoEXT>();
            debugCreateInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = &debugCallback;
        }
        void destroyDebugMessenger() noexcept
        {
            if ((debug_messenger != nullptr) && (instance != nullptr))
            { // NOLINTNEXTLINE
                auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    ::vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
                if (func != nullptr)
                {
                    func(instance, debug_messenger, nullptr);
                }
                debug_messenger = VK_NULL_HANDLE;
            }
        }

        // ---------------------cpp like----------------------------------
        [[nodiscard]] auto enumeratePhysicalDevices() const
        {
            uint32_t deviceCount; // NOLINT
            ::vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
            if (deviceCount == 0)
                throw std::runtime_error("failed to find GPUs with Vulkan support!");
            std::vector<VkPhysicalDevice> gpus(deviceCount);
            ::vkEnumeratePhysicalDevices(instance, &deviceCount, gpus.data());
            return gpus;
        }
        static auto getPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice &gpu)
        {
            uint32_t queueFamilyCount = 0;
            ::vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            ::vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount,
                                                       queueFamilies.data());
            return queueFamilies;
        }
        static auto enumerateDeviceExtensionProperties(const VkPhysicalDevice &gpu)
        {
            // Check if all required device extensions are available
            uint32_t extensionCount; // NOLINT
            ::vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount,
                                                   nullptr);
            std::vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
            ::vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount,
                                                   availableDeviceExtensions.data());
            return availableDeviceExtensions;
        }
        static auto getPhysicalDeviceProperties(const VkPhysicalDevice &gpu)
        {
            VkPhysicalDeviceProperties deviceProperties;
            ::vkGetPhysicalDeviceProperties(gpu, &deviceProperties);
            return deviceProperties;
        }
        // NOTE: 2. 检查是否支持，我们要求的特性
        void checkFeatureSupport()
        {
            // NOTE: 根据 appInfo.profileSupported 来创建和配置
            // Check for Vulkan profile support
            VpProfileProperties profileProperties = {
                .profileName = VP_KHR_ROADMAP_2022_NAME,
                .specVersion = VP_KHR_ROADMAP_2022_SPEC_VERSION};

            VkBool32 supported; // NOLINT
            VkResult vk_result = ::vpGetPhysicalDeviceProfileSupport(
                instance, physicalDevice, &profileProperties, &supported);
            if (vk_result == VkResult::VK_SUCCESS && supported == VK_TRUE)
            {
                appInfo.profileSupported = true; // TODO(mcs): 如果不支持应该回退
                appInfo.profile = profileProperties;
                LOGI("Device supports Vulkan profile: {}", profileProperties.profileName);
            }
            else
            {
                LOGW("Device does not support Vulkan profile: {}",
                     profileProperties.profileName);
            }
        }
        [[nodiscard]] auto getPhysicalDeviceSurfaceSupportKHR(
            uint32_t queueFamilyIndex, VkSurfaceKHR surface) const noexcept
        {
            VkBool32 presentSupport; // NOLINT
            ::vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex,
                                                   surface, &presentSupport);
            return presentSupport;
        }

        //---------------------------------
        void createInstance(VkApplicationInfo &appInfo)
        {
            // c1: 1. extensions
            auto available_instance_extensions = availableInstanceExtension();
            std::vector<const char *> required_instance_extensions{};
            addSurfaceExtension(required_instance_extensions);
            addDebugExtension(required_instance_extensions);
            if (!checkExtensionSupport(required_instance_extensions,
                                       available_instance_extensions))
                throw std::runtime_error("Required instance extensions are missing.");

            // c1: 2. layers
            auto supported_instance_layers = availableInstanceLayer();
            std::vector<const char *> requested_instance_layers{};
            bool has_debug_utils = addDebuglayerIfAvailable(requested_instance_layers,
                                                            supported_instance_layers);
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
            std::vector<const char *> validationLayers = {};
            if (enableValidationLayers && has_debug_utils)
            {
                validationLayers.emplace_back(VK_DEBUG_LAYER_NAME);
                applyDebugCreateInfo(debugCreateInfo);
            }

            static_assert(vulkan::sType<VkInstanceCreateInfo>() ==
                          VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);

            // NOTE: 必须在创建的时候关联 扩展，才启动扩展
            VkInstanceCreateInfo createInfo = {
                .sType = vulkan::sType<VkInstanceCreateInfo>(),
                .pApplicationInfo = &appInfo,
                .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
                .ppEnabledLayerNames = validationLayers.data(),
                .enabledExtensionCount =
                    static_cast<uint32_t>(required_instance_extensions.size()),
                .ppEnabledExtensionNames = required_instance_extensions.data(),
            };
            // set debuger
            if (enableValidationLayers && has_debug_utils)
                createInfo.pNext = &debugCreateInfo; // c1: create with Debug

            // c1: 3. CreateInstance
            checkVkResult(::vkCreateInstance(&createInfo, nullptr, &instance));
        }

        constexpr void createSurface(window_type &window)
        {
            window.createVkSurfaceKHR(instance, surface);
        }

        [[nodiscard]] VkSampleCountFlagBits getMaxUsableSampleCount() const noexcept
        {
            VkPhysicalDeviceProperties physicalDeviceProperties =
                getPhysicalDeviceProperties(physicalDevice);
            auto counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                          physicalDeviceProperties.limits.framebufferDepthSampleCounts;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_64_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_64_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_32_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_32_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_16_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_16_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT;
            if ((counts & VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT) != 0U)
                return VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT;
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
        }
        void pickPhysicalDevice(const std::vector<const char *> &requiredDeviceExtension)
        {
            std::vector<VkPhysicalDevice> gpus = enumeratePhysicalDevices();

            const auto k_iter = std::ranges::find_if(gpus, [&](auto const &gpu) noexcept {
                // Check queue family support
                std::vector<VkQueueFamilyProperties> queueFamilies =
                    getPhysicalDeviceQueueFamilyProperties(gpu);
                bool supportsGraphics = std::ranges::any_of(
                    queueFamilies, [](auto const &queueFamily) noexcept {
                        return (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                    }); // NOTE: 这里查找可以变量输入

                // Check if all required device extensions are available
                std::vector<VkExtensionProperties> availableDeviceExtensions =
                    enumerateDeviceExtensionProperties(gpu);

                bool supportsAllRequiredExtensions = std::ranges::all_of(
                    requiredDeviceExtension,
                    [&availableDeviceExtensions](
                        auto const &requiredDeviceExtension) noexcept {
                        return std::ranges::any_of(
                            availableDeviceExtensions,
                            [&](auto const &availableDeviceExtension) noexcept {
                                return ::strcmp(availableDeviceExtension.extensionName,
                                                requiredDeviceExtension) == 0;
                            });
                    });
                return supportsGraphics && supportsAllRequiredExtensions;
            });
            if (k_iter != gpus.end())
            {
                physicalDevice = *k_iter;
                msaaSamples = getMaxUsableSampleCount();
                VkPhysicalDeviceProperties deviceProperties =
                    getPhysicalDeviceProperties(physicalDevice);
                LOGI("Selected GPU: {}. API Version: {}.{}.{}",
                     deviceProperties.deviceName,
                     VK_VERSION_MAJOR(deviceProperties.apiVersion),
                     VK_VERSION_MINOR(deviceProperties.apiVersion),
                     VK_VERSION_PATCH(deviceProperties.apiVersion));

                checkFeatureSupport(); // c2: profies check
                return;
            }
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        void createLogicalDevice(const std::vector<const char *> &requiredDeviceExtension)
        {

            std::vector<VkQueueFamilyProperties> queueFamilies =
                getPhysicalDeviceQueueFamilyProperties(physicalDevice);

            for (uint32_t i = 0, queueFamilyCount = queueFamilies.size();
                 i < queueFamilyCount; i++)
            {
                if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U)
                {
                    if (getPhysicalDeviceSurfaceSupportKHR(i, surface) != 0U)
                    {
                        queueIndex = i;
                        break;
                    }
                }
            }
            if (queueIndex == ~0)
                throw std::runtime_error(
                    "Could not find a queue for graphics and present -> terminating");

            float queuePriority = 1.0F;
            VkDeviceQueueCreateInfo queueCreateInfo = {
                .sType = vulkan::sType<VkDeviceQueueCreateInfo>(),
                .queueFamilyIndex = queueIndex,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority};

            structure_chain<VkPhysicalDeviceFeatures2, VkPhysicalDeviceVulkan11Features,
                            VkPhysicalDeviceVulkan13Features,
                            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>
                featureChain = {
                    {},
                    {.shaderDrawParameters = VK_TRUE},
                    {.synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE},
                    {.extendedDynamicState = VK_TRUE}};

            static_assert(std::is_same_v<decltype(auto(featureChain.head())),
                                         VkPhysicalDeviceFeatures2>);
            assert(featureChain.size() == requiredDeviceExtension.size());

            VkDeviceCreateInfo createInfo = {
                .sType = vulkan::sType<VkDeviceCreateInfo>(),
                .pNext = &featureChain.head(),
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &queueCreateInfo,
                .enabledExtensionCount =
                    static_cast<uint32_t>(requiredDeviceExtension.size()),
                .ppEnabledExtensionNames = requiredDeviceExtension.data(),
                // .pEnabledFeatures = "";
            };

            if (::vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create logical device!");
            ::vkGetDeviceQueue(device, queueIndex, 0, &queue);
        }

      public:
        void setup(VkApplicationInfo &appInfo, window_type &window)
        {
            createInstance(appInfo);
            createSurface(window);

            /*
Querying Properties, Extensions, Features, Limits, and Formats
NOTE: Properties 只读数据的抽象
NOTE: Extensions：扩展可以添加新功能
NOTE: Features：特性描述了并非所有实现都支持的功能
Limits：限制是应用程序可能需要注意的implementation-dependent最小值、最大值和其他设备特性
NOTE: Formats: 布局
*/
            std::vector<const char *> requiredDeviceExtension = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME,
                VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
                VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME}; // NOLINTEND

            pickPhysicalDevice(requiredDeviceExtension);
            createLogicalDevice(requiredDeviceExtension);
        }
        void teardown()
        {

            // Cleanup surface
            if ((surface != nullptr) && (instance != nullptr))
            {
                ::vkDestroySurfaceKHR(instance, surface, nullptr);
                surface = VK_NULL_HANDLE;
            }

            // Cleanup device
            if (device != nullptr)
            {
                ::vkDestroyDevice(device, nullptr);
                device = VK_NULL_HANDLE;
            }

            // Cleanup debug messenger
            destroyDebugMessenger();

            // Cleanup instance
            if (instance != nullptr)
            {
                ::vkDestroyInstance(instance, nullptr);
                instance = VK_NULL_HANDLE;
            }
        }
    };

    struct presentation
    {
        // NOLINTBEGIN
        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        std::vector<VkImage> swapChainImages;
        VkSurfaceFormatKHR swapChainSurfaceFormat{};
        VkExtent2D swapChainExtent{};
        std::vector<VkImageView> swapChainImageViews; // NOLINTEND

        vulkan_context *context{}; // NOLINT
        using window_type = vulkan_context::window_type;
        window_type *window{}; // NOLINT

      private:
        static uint32_t chooseSwapMinImageCount(
            const VkSurfaceCapabilitiesKHR &capabilities)
        {
            assert(capabilities.maxImageCount != 0);
            uint32_t minImageCount = capabilities.minImageCount + 1;
            if (capabilities.maxImageCount > 0 &&
                minImageCount > capabilities.maxImageCount)
                minImageCount = capabilities.maxImageCount;
            return minImageCount;
        }
        static VkSurfaceFormatKHR chooseSwapSurfaceFormat(
            const std::vector<VkSurfaceFormatKHR> &availableFormats)
        {
            VkSurfaceFormatKHR select = {.format = VK_FORMAT_B8G8R8A8_SRGB,
                                         .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
            for (const auto &availableFormat : availableFormats)
            {
                if (availableFormat.format == select.format &&
                    availableFormat.colorSpace == select.colorSpace)
                    return availableFormat;
            }
            return availableFormats[0];
        }
        static VkPresentModeKHR chooseSwapPresentMode(
            const std::vector<VkPresentModeKHR> &availablePresentModes)
        {
            VkPresentModeKHR select = VK_PRESENT_MODE_MAILBOX_KHR;
            for (const auto &availablePresentMode : availablePresentModes)
            {
                if (availablePresentMode == select)
                {
                    return availablePresentMode;
                }
            }
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        static VkExtent2D chooseSwapExtent(window_type &window,
                                           const VkSurfaceCapabilitiesKHR &capabilities)
        {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
                return capabilities.currentExtent;

            auto [width, height] = window.getFramebufferSize();
            return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width),
                    std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height)};
        }

        //-------------------------cpp like api------------------------------
        constexpr static auto getPhysicalDeviceSurfaceCapabilitiesKHR(
            VkPhysicalDevice &physicalDevice, VkSurfaceKHR &surface) noexcept
        {
            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            ::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                        &surfaceCapabilities);
            return surfaceCapabilities;
        }
        constexpr static auto getPhysicalDeviceSurfaceFormatsKHR(
            VkPhysicalDevice &physicalDevice, VkSurfaceKHR &surface)
        {
            uint32_t formatCount; // NOLINT
            checkVkResult(::vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                                                 &formatCount, nullptr));
            std::vector<VkSurfaceFormatKHR> formats(formatCount);
            checkVkResult(::vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice, surface, &formatCount, formats.data()));
            return formats;
        }
        constexpr static auto getPhysicalDeviceSurfacePresentModesKHR(
            VkPhysicalDevice &physicalDevice, VkSurfaceKHR &surface)
        {
            uint32_t presentModeCount; // NOLINT
            checkVkResult(::vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &presentModeCount, nullptr));
            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            checkVkResult(::vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &presentModeCount, presentModes.data()));
            return presentModes;
        }
        constexpr static auto getSwapchainImagesKHR(VkDevice &device,
                                                    VkSwapchainKHR &swapchain)
        {
            uint32_t imageCount; // NOLINT
            std::vector<VkImage> associatedImages;
            ::vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
            associatedImages.resize(imageCount);
            ::vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                                      associatedImages.data());
            return associatedImages;
        }
        //-------------------------------------------------------------------

        void createSwapChain()
        {
            auto &physicalDevice = context->physicalDevice;
            auto &surface = context->surface;
            auto &device = context->device;
            auto &window = *(this->window);

            VkSurfaceCapabilitiesKHR surfaceCapabilities =
                getPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface);

            std::vector<VkSurfaceFormatKHR> availableFormats =
                getPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface);

            std::vector<VkPresentModeKHR> availablePresentModes =
                getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface);

            swapChainExtent = chooseSwapExtent(window, surfaceCapabilities);
            swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);
            uint32_t imageCount = chooseSwapMinImageCount(surfaceCapabilities);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

            VkSwapchainCreateInfoKHR createInfo = {
                .sType = vulkan::sType<VkSwapchainCreateInfoKHR>(),
                .surface = surface,
                .minImageCount = imageCount,
                .imageFormat = swapChainSurfaceFormat.format,
                .imageColorSpace = swapChainSurfaceFormat.colorSpace,
                .imageExtent = swapChainExtent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .preTransform = surfaceCapabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = presentMode,
                .clipped = VK_TRUE};

            if (::vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create swap chain!");

            swapChainImages = getSwapchainImagesKHR(device, swapChain);
        }

        void createImageViews()
        {
            auto &device = context->device;

            swapChainImageViews.resize(swapChainImages.size());
            for (size_t i = 0; i < swapChainImages.size(); i++)
            {
                VkImageViewCreateInfo createInfo = {
                    .sType = vulkan::sType<VkImageViewCreateInfo>(),
                    .image = swapChainImages[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = swapChainSurfaceFormat.format,
                    .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                   .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                   .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                   .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                    .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .baseMipLevel = 0,
                                         .levelCount = 1,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1}};

                if (::vkCreateImageView(device, &createInfo, nullptr,
                                        &swapChainImageViews[i]) != VK_SUCCESS)
                    throw std::runtime_error("failed to create image views!");
            }
        }
        void cleanupSwapChain()
        {
            auto &device = context->device;

            // 销毁图像视图
            for (auto *imageView : swapChainImageViews)
                ::vkDestroyImageView(device, imageView, nullptr);
            swapChainImageViews.clear();

            // 销毁交换链（这会自动销毁相关的VkImage）
            if (swapChain != nullptr)
            {
                ::vkDestroySwapchainKHR(device, swapChain, nullptr);
                swapChain = VK_NULL_HANDLE;
            }
        }

      public:
        void setup(vulkan_context &ctx, window_type &w)
        {
            context = &ctx;
            window = &w;
            createSwapChain();
            createImageViews();
        }
        void teardown() noexcept
        {
            cleanupSwapChain();
        }
        void recreateSwapChain()
        {
            auto &device = context->device;
            window->waitGoodFramebufferSize();
            ::vkDeviceWaitIdle(device);

            cleanupSwapChain();

            createSwapChain();
            createImageViews();
        }
    };

    struct graphics_pipeline
    {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE; // NOLINT
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;     // NOLINT

        vulkan_context *context{}; // NOLINT

      private:
        static std::vector<char> readFile(const std::string &filename)
        {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);
            if (!file.is_open())
            {
                throw std::runtime_error("failed to open file!");
            }
            std::vector<char> buffer(file.tellg());
            file.seekg(0, std::ios::beg);
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            file.close();
            return buffer;
        }
        [[nodiscard]] VkShaderModule createShaderModule(
            const std::vector<char> &code) const
        {
            auto &device = context->device;

            VkShaderModuleCreateInfo createInfo = {
                .sType = vulkan::sType<VkShaderModuleCreateInfo>(),
                .codeSize = code.size(),
                .pCode = reinterpret_cast<const uint32_t *>(code.data())}; // NOLINT

            VkShaderModule shaderModule; // NOLINT
            if (::vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create shader module!");

            return shaderModule;
        }

      public:
        void createGraphicsPipeline(const VkSurfaceFormatKHR &swapChainSurfaceFormat)
        {
            // auto &swapChainSurfaceFormat = swapchain.swapChainSurfaceFormat;
            auto &device = context->device;

            VkShaderModule shaderModule =
                createShaderModule(readFile("shaders/09_shader_base.spv"));
            VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
                .sType = vulkan::sType<VkPipelineShaderStageCreateInfo>(),
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = shaderModule,
                .pName = "vertMain"};
            VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
                .sType = vulkan::sType<VkPipelineShaderStageCreateInfo>(),
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = shaderModule,
                .pName = "fragMain"};

            // NOLINTNEXTLINE
            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                              fragShaderStageInfo};

            VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
                .sType = vulkan::sType<VkPipelineVertexInputStateCreateInfo>()};

            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
                .sType = vulkan::sType<VkPipelineInputAssemblyStateCreateInfo>(),
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE};

            VkPipelineViewportStateCreateInfo viewportState = {
                .sType = vulkan::sType<VkPipelineViewportStateCreateInfo>(),
                .viewportCount = 1,
                .scissorCount = 1};

            VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType = vulkan::sType<VkPipelineRasterizationStateCreateInfo>(),
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0F};

            VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType = vulkan::sType<VkPipelineMultisampleStateCreateInfo>(),
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable = VK_FALSE,
            };
            VkPipelineColorBlendAttachmentState colorBlendAttachment = {
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

            VkPipelineColorBlendStateCreateInfo colorBlending = {
                .sType = vulkan::sType<VkPipelineColorBlendStateCreateInfo>(),
                .logicOpEnable = VK_FALSE,
                .attachmentCount = 1,
                .pAttachments = &colorBlendAttachment};

            // NOTE: 动态的意思是 cmd 的时候需要指定，确定动态类型
            std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY};
            VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = vulkan::sType<VkPipelineDynamicStateCreateInfo>(),
                .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates = dynamicStates.data()};

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = vulkan::sType<VkPipelineLayoutCreateInfo>()};

            if (::vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                         &pipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create pipeline layout!");
            }
            // Use dynamic rendering
            vulkan::structure_chain<VkGraphicsPipelineCreateInfo,
                                    VkPipelineRenderingCreateInfo>
                pipelineCreateInfoChain = {
                    {.stageCount = 2,
                     .pStages = shaderStages,
                     .pVertexInputState = &vertexInputInfo,
                     .pInputAssemblyState = &inputAssembly,
                     .pViewportState = &viewportState,
                     .pRasterizationState = &rasterizer,
                     .pMultisampleState = &multisampling,
                     .pColorBlendState = &colorBlending,
                     .pDynamicState = &dynamicState,
                     .layout = pipelineLayout,
                     .renderPass = VK_NULL_HANDLE},
                    {.colorAttachmentCount = 1,
                     .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

            if (::vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                            &pipelineCreateInfoChain.head(), nullptr,
                                            &graphicsPipeline) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create graphics pipeline!");
            }

            ::vkDestroyShaderModule(device, shaderModule, nullptr);
        }

        void setup(vulkan_context &ctx, const VkSurfaceFormatKHR &swapChainSurfaceFormat)
        {
            context = &ctx;
            createGraphicsPipeline(swapChainSurfaceFormat);
        }
        void teardown() noexcept
        {
            auto &device = context->device;
            // Cleanup pipeline
            if (graphicsPipeline != nullptr)
            {
                ::vkDestroyPipeline(device, graphicsPipeline, nullptr);
                graphicsPipeline = VK_NULL_HANDLE;
            }
            if (pipelineLayout != nullptr)
            {
                ::vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
                pipelineLayout = VK_NULL_HANDLE;
            }
        }
    };

    struct render
    {
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2; // NOLINT

        struct per_frame // NOLINTBEGIN
        {
            VkCommandBuffer commandBuffers;
            VkSemaphore presentCompleteSemaphore;
            VkSemaphore renderFinishedSemaphore;
            VkFence inFlightFences;
        }; // NOLINTEND

        // NOLINTBEGIN
        VkCommandPool commandPool = VK_NULL_HANDLE;
        // std::vector<per_frame> frames;
        std::vector<VkCommandBuffer> commandBuffers;
        std::vector<VkSemaphore> presentCompleteSemaphore;
        std::vector<VkSemaphore> renderFinishedSemaphore;
        std::vector<VkFence> inFlightFences;
        uint32_t semaphoreIndex = 0;
        uint32_t currentFrame = 0; // NOLINTEND

        vulkan::presentation *presentation{};  // NOLINT
        vulkan::graphics_pipeline *pipeline{}; // NOLINT

      public:
        void createCommandPool()
        {
            auto &queueIndex = presentation->context->queueIndex;
            auto &device = presentation->context->device;

            VkCommandPoolCreateInfo poolInfo = {
                .sType = vulkan::sType<VkCommandPoolCreateInfo>(),
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queueIndex};

            if (::vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to create command pool!");
            }
        }
        void createCommandBuffers()
        {
            auto &device = presentation->context->device;

            commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

            VkCommandBufferAllocateInfo allocInfo = {
                .sType = vulkan::sType<VkCommandBufferAllocateInfo>(),
                .commandPool = commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())};

            if (::vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to allocate command buffers!");
            }
        }
        void createSyncObjects()
        {
            auto &device = presentation->context->device;
            auto &swapChainImages = presentation->swapChainImages;

            presentCompleteSemaphore.resize(swapChainImages.size());
            renderFinishedSemaphore.resize(swapChainImages.size());
            VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = vulkan::sType<VkSemaphoreCreateInfo>()};

            for (size_t i = 0; i < swapChainImages.size(); i++)
            {
                if (::vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                        &presentCompleteSemaphore[i]) != VK_SUCCESS ||
                    ::vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                                        &renderFinishedSemaphore[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create semaphores!");
                }
            }

            VkFenceCreateInfo fenceInfo = {.sType = vulkan::sType<VkFenceCreateInfo>(),
                                           .flags = VK_FENCE_CREATE_SIGNALED_BIT};
            inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                if (::vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) !=
                    VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create fence!");
                }
            }
        }

        void recordCommandBuffer(uint32_t imageIndex)
        {
            auto &swapChainImages = presentation->swapChainImages;
            auto &swapChainImageViews = presentation->swapChainImageViews;
            auto &swapChainExtent = presentation->swapChainExtent;
            auto &graphicsPipeline = pipeline->graphicsPipeline;

            VkCommandBufferBeginInfo beginInfo = {
                .sType = vulkan::sType<VkCommandBufferBeginInfo>()};

            if (::vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // Transition image layout for rendering
            transitionImageLayout(
                commandBuffers[currentFrame], swapChainImages[imageIndex],
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            VkClearValue clearColor = {.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}};
            VkRenderingAttachmentInfo colorAttachment = {
                .sType = vulkan::sType<VkRenderingAttachmentInfo>(),
                .imageView = swapChainImageViews[imageIndex],
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = clearColor};

            VkRenderingInfo renderingInfo = {
                .sType = vulkan::sType<VkRenderingInfo>(),
                .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment};

            ::vkCmdBeginRendering(commandBuffers[currentFrame], &renderingInfo);

            ::vkCmdBindPipeline(commandBuffers[currentFrame],
                                VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkViewport viewport = {.x = 0.0F,
                                   .y = 0.0F,
                                   .width = static_cast<float>(swapChainExtent.width),
                                   .height = static_cast<float>(swapChainExtent.height),
                                   .minDepth = 0.0F,
                                   .maxDepth = 1.0F};
            ::vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

            VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent};
            ::vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

            // Since we declared VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY as dynamic,
            // we need to set the primitive topology here.
            // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST tells Vulkan that the input vertex data
            // should be interpreted as a list of triangles.
            ::vkCmdSetPrimitiveTopology(commandBuffers[currentFrame],
                                        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

            ::vkCmdDraw(commandBuffers[currentFrame], 3, 1, 0, 0);
            ::vkCmdEndRendering(commandBuffers[currentFrame]);

            // Transition image layout for presentation
            transitionImageLayout(
                commandBuffers[currentFrame], swapChainImages[imageIndex],
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

            if (::vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to record command buffer!");
            }
        }

        static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                          VkImageLayout oldLayout,
                                          VkImageLayout newLayout,
                                          VkAccessFlags srcAccessMask,
                                          VkAccessFlags dstAccessMask, // NOLINT
                                          VkPipelineStageFlags srcStageMask,
                                          VkPipelineStageFlags dstStageMask)
        {
            VkImageMemoryBarrier barrier = {
                .sType = vulkan::sType<VkImageMemoryBarrier>(),
                // Specify the pipeline stages and access masks for the barrier
                .srcAccessMask = srcAccessMask,
                .dstAccessMask = dstAccessMask,
                // Specify the old and new layouts of the image
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                // We are not changing the ownership between queues
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                // Specify the image to be affected by this barrier
                .image = image,
                // Define the subresource range (which parts of the image are affected)
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .baseMipLevel = 0,
                                     .levelCount = 1,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1}};
            ::vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0,
                                   nullptr, 0, nullptr, 1, &barrier);
        }

        void drawFrame()
        {
            auto &swapChain = presentation->swapChain;
            auto &device = presentation->context->device;
            auto &queue = presentation->context->queue;
            auto &window = presentation->window;

            ::vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE,
                              UINT64_MAX);

            uint32_t imageIndex; // NOLINT
            VkResult result = ::vkAcquireNextImageKHR(
                device, swapChain, UINT64_MAX, presentCompleteSemaphore[semaphoreIndex],
                VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                presentation->recreateSwapChain();
                return;
            }
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            ::vkResetFences(device, 1, &inFlightFences[currentFrame]);
            ::vkResetCommandBuffer(commandBuffers[currentFrame], 0);
            recordCommandBuffer(imageIndex);

            // NOLINTNEXTLINE
            VkPipelineStageFlags waitStages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

            VkSubmitInfo submitInfo = {
                .sType = vulkan::sType<VkSubmitInfo>(),
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &presentCompleteSemaphore[semaphoreIndex],
                .pWaitDstStageMask = waitStages,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffers[currentFrame],
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &renderFinishedSemaphore[imageIndex]};

            if (::vkQueueSubmit(queue, 1, &submitInfo, inFlightFences[currentFrame]) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            VkPresentInfoKHR presentInfo = {.sType = vulkan::sType<VkPresentInfoKHR>(),
                                            .waitSemaphoreCount = 1,
                                            .pWaitSemaphores =
                                                &renderFinishedSemaphore[imageIndex],
                                            .swapchainCount = 1,
                                            .pSwapchains = &swapChain,
                                            .pImageIndices = &imageIndex};

            result = ::vkQueuePresentKHR(queue, &presentInfo);

            if (auto &framebufferResized = window->refFramebufferResized();
                result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
                framebufferResized)
            {
                framebufferResized = false;
                presentation->recreateSwapChain();
            }
            else if (result != VK_SUCCESS)
                throw std::runtime_error("failed to present swap chain image!");

            semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        void setup(vulkan::presentation &presentation,
                   vulkan::graphics_pipeline &pipeline)
        {
            this->presentation = &presentation;
            this->pipeline = &pipeline;
            createCommandPool();
            createCommandBuffers();
            createSyncObjects();
        }

        void teardown() noexcept
        {
            auto &device = presentation->context->device;
            // Cleanup sync objects
            for (auto *semaphore : presentCompleteSemaphore)
                ::vkDestroySemaphore(device, semaphore, nullptr);
            presentCompleteSemaphore.clear();
            for (auto *semaphore : renderFinishedSemaphore)
                ::vkDestroySemaphore(device, semaphore, nullptr);
            renderFinishedSemaphore.clear();
            for (auto *fence : inFlightFences)
                ::vkDestroyFence(device, fence, nullptr);
            inFlightFences.clear();

            // Cleanup command buffers and pool
            if (commandPool != nullptr)
            {
                ::vkDestroyCommandPool(device, commandPool, nullptr);
                commandPool = VK_NULL_HANDLE;
            }
        }
    };
};

// NOLINTBEGIN

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
    glfw::Window window{};
    vulkan::vulkan_context context;
    vulkan::presentation presentation;
    vulkan::graphics_pipeline pipeline;
    vulkan::render render;

    void initWindow()
    {
        constexpr uint32_t WIDTH = 800;
        constexpr uint32_t HEIGHT = 600;
        window.setup(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        VkApplicationInfo appInfo = {.sType = vulkan::sType<VkApplicationInfo>(),
                                     .pApplicationName = "Hello Triangle",
                                     .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                     .pEngineName = "No Engine",
                                     .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                     // apiVersion必须是应用程序设计使用的Vulkan的最高版本
                                     .apiVersion = VK_API_VERSION_1_3};
        context.setup(appInfo, window);
        presentation.setup(context, window);
        pipeline.setup(context, presentation.swapChainSurfaceFormat);
        render.setup(presentation, pipeline);
    }

    void mainLoop()
    {
        while (!window.shouldClose())
        {
            window.pollEvents();
            render.drawFrame();
        }
        // Don't release anything until the GPU is completely idle.
        ::vkDeviceWaitIdle(context.device);
    }

    void cleanup() noexcept
    {
        render.teardown();

        // Cleanup pipeline
        pipeline.teardown();
        presentation.teardown();

        context.teardown();
        window.teardown();
    }
};

int main()
{
    vulkan::vulkan_context::printAvailableInstanceExtension();
    // NOTE: 相比于 hello_triangle2 实际增加的代码并不多。框架的代码也是很多的
    // NOTE: 可读性可以自己提升，没那么难
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