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
            glfwSetScrollCallback(window_, scrollCallback);

            // 隐藏并捕获光标以实现平滑的相机旋转
            // ::glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

            // 初始化输入状态
            inputState.leftMousePressed = false;
            inputState.rightMousePressed = false;
            inputState.middleMousePressed = false;
            inputState.firstMouse = true;
            inputState.lastX = static_cast<float>(width) / 2.0f;  // NOLINT
            inputState.lastY = static_cast<float>(height) / 2.0f; // NOLINT
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
        void waitGoodFramebufferSize() const noexcept
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

        // 新增：获取输入状态
        [[nodiscard]] const auto &getInputState() const noexcept
        {
            return inputState;
        }
        [[nodiscard]] auto &refInputState() noexcept
        {
            return inputState;
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
            if (::glfwCreateWindowSurface(instance, data(), nullptr, &surface) !=
                VK_SUCCESS)
                throw std::runtime_error("failed to create window surface!");
        }

      private:
        window_pointer window_ = nullptr;
        bool framebufferResized_{};

        // 输入状态结构体 // NOLINTBEGIN
        struct InputState
        {
            bool leftMousePressed = false;
            bool rightMousePressed = false;
            bool middleMousePressed = false;
            bool firstMouse = true;
            float lastX = 0.0f;
            float lastY = 0.0f;
            float xOffset = 0.0f;
            float yOffset = 0.0f;
            float scrollOffset = 0.0f;

            // 键盘状态
            bool keyW = false;
            bool keyS = false;
            bool keyA = false;
            bool keyD = false;
            bool keyQ = false;
            bool keyE = false;

            // 鼠标拖动
            float dragDeltaX = 0.0f;
            float dragDeltaY = 0.0f;

            // 清空每帧的增量
            void clearFrameDeltas()
            {
                xOffset = 0.0f;
                yOffset = 0.0f;
                scrollOffset = 0.0f;
                dragDeltaX = 0.0f;
                dragDeltaY = 0.0f;
            }
        } inputState; // NOLINTEND

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
            auto *app = static_cast<Window *>(::glfwGetWindowUserPointer(window));
            auto &input = app->inputState;

            // 处理 ESC 键：退出程序
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            {
                std::cout << "KEY GLFW_KEY_ESCAPE: PRESS" << '\n';
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            // 处理空格键
            if (key == GLFW_KEY_SPACE)
            {
                if (action == GLFW_PRESS)
                {
                    std::cout << "KEY SPACE: PRESSED" << '\n';
                }
                else if (action == GLFW_RELEASE)
                {
                    std::cout << "KEY SPACE: RELEASED" << '\n';
                }
                else if (action == GLFW_REPEAT)
                {
                    std::cout << "KEY SPACE: REPEATING" << '\n';
                }
            }

            // 处理 QWEASD 按键
            bool value = (action == GLFW_PRESS || action == GLFW_REPEAT);
            switch (key)
            {
            case GLFW_KEY_W:
                input.keyW = value;
                break;
            case GLFW_KEY_S:
                input.keyS = value;
                break;
            case GLFW_KEY_A:
                input.keyA = value;
                break;
            case GLFW_KEY_D:
                input.keyD = value;
                break;
            case GLFW_KEY_Q:
                input.keyQ = value;
                break;
            case GLFW_KEY_E:
                input.keyE = value;
                break;
            }
        }

        // Callback for mouse movement
        static void cursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
        {
            auto *app = static_cast<Window *>(::glfwGetWindowUserPointer(window));
            auto &input = app->inputState;

            if (input.firstMouse)
            {
                input.lastX = static_cast<float>(xpos);
                input.lastY = static_cast<float>(ypos);
                input.firstMouse = false;
            }

            // 计算鼠标移动偏移量
            input.xOffset = static_cast<float>(xpos) - input.lastX;
            input.yOffset = static_cast<float>(ypos) - input.lastY;

            // 如果正在拖动，记录拖动增量
            if (input.leftMousePressed || input.rightMousePressed ||
                input.middleMousePressed)
            {
                input.dragDeltaX = input.xOffset;
                input.dragDeltaY = input.yOffset;
            }

            input.lastX = static_cast<float>(xpos);
            input.lastY = static_cast<float>(ypos);
        }

        // Callback for mouse buttons
        static void mouseButtonCallback(GLFWwindow *window, int button, int action,
                                        int mods)
        {
            auto *app = static_cast<Window *>(::glfwGetWindowUserPointer(window));
            auto &input = app->inputState;

            bool pressed = (action == GLFW_PRESS);

            // NOTE:鼠标中键无用，滚轮有用
            //  添加详细调试信息
            //  std::cout << "Mouse button callback - Button: " << button
            //            << ", Action: " << action << ", Mods: " << mods << std::endl;
            //  // 打印具体的按钮名称
            //  if (button == GLFW_MOUSE_BUTTON_LEFT)
            //      std::cout << "  LEFT button" << std::endl;
            //  else if (button == GLFW_MOUSE_BUTTON_RIGHT)
            //      std::cout << "  RIGHT button" << std::endl;
            //  else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
            //      std::cout << "  MIDDLE button" << std::endl;
            //  else
            //      std::cout << "  OTHER button: " << button << std::endl;

            if (button == GLFW_MOUSE_BUTTON_LEFT)
            {
                input.leftMousePressed = pressed;
                if (pressed)
                {
                    // 重置第一次鼠标标志，避免跳跃
                    input.firstMouse = true;
                }
            }
            else if (button == GLFW_MOUSE_BUTTON_RIGHT)
            {
                input.rightMousePressed = pressed;
                if (pressed)
                {
                    // 重置第一次鼠标标志，避免跳跃
                    input.firstMouse = true;
                }
            }
            else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
            {
                input.middleMousePressed = pressed;
                if (pressed)
                {
                    // 重置第一次鼠标标志，避免跳跃
                    input.firstMouse = true;
                }
            }
        }

        // Callback for mouse scroll
        static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
        {
            auto *app = static_cast<Window *>(::glfwGetWindowUserPointer(window));
            auto &input = app->inputState;

            input.scrollOffset = static_cast<float>(yoffset);
        }
        // NOLINTEND
    };

}; // namespace glfw