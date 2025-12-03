#pragma once

#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

#define GLFW_INCLUDE_VULKAN // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

#include <vulkan/vulkan_core.h>

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
