#pragma once
#include "shadertoy_uniforms.hpp"
#include <glm/glm.hpp>
#include <chrono>

// NOLINTBEGIN
class ShadertoyRenderer
{
  private:
    // 内部状态
    shadertoy_uniforms uniforms_;

    // 计时相关
    std::chrono::high_resolution_clock::time_point startTime_;
    std::chrono::high_resolution_clock::time_point lastFrameTime_;
    float accumulatedTime_ = 0.0F;
    int frameCount_ = 0;

    // 鼠标状态
    glm::vec2 mousePos_ = {0.0F, 0.0F};
    glm::vec2 mouseClickPos_ = {0.0F, 0.0F};
    bool mouseDown_ = false;

    // 屏幕尺寸
    glm::vec2 screenSize_ = {0.0F, 0.0F};

  public:
    ShadertoyRenderer()
        : startTime_{std::chrono::high_resolution_clock::now()},
          lastFrameTime_(startTime_)
    {

        // 初始化日期
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        uniforms_.iDate =
            glm::vec4(tm.tm_year + 1900, // year
                      tm.tm_mon + 1,     // month
                      tm.tm_mday,        // day
                      std::chrono::duration<float>(now.time_since_epoch() %
                                                   std::chrono::seconds(86400))
                          .count());
    }

    void updateUniforms(uint32_t width, uint32_t height)
    {
        auto currentTime = std::chrono::high_resolution_clock::now();

        // 更新时间
        float time = std::chrono::duration<float>(currentTime - startTime_).count();
        float frameTime =
            std::chrono::duration<float>(currentTime - lastFrameTime_).count();

        uniforms_.iTime = time;
        uniforms_.iTimeDelta = frameTime;

        // 更新帧率
        accumulatedTime_ += frameTime;
        frameCount_++;

        if (accumulatedTime_ >= 1.0f)
        {
            uniforms_.iFrameRate = static_cast<float>(frameCount_) / accumulatedTime_;
            frameCount_ = 0;
            accumulatedTime_ = 0.0f;
        }

        // 更新帧数
        uniforms_.iFrame++;

        // 更新分辨率
        uniforms_.iResolution = glm::vec3(width, height, 0.0f);

        // 更新鼠标
        uniforms_.iMouse =
            glm::vec4(mousePos_.x, screenSize_.y - mousePos_.y, // Shadertoy坐标系：y向下
                      mouseDown_ ? mouseClickPos_.x : 0.0F,
                      mouseDown_ ? screenSize_.y - mouseClickPos_.y : 0.0F);

        lastFrameTime_ = currentTime;
        screenSize_ = glm::vec2(width, height);
    }

    [[nodiscard]] const auto &getUniforms() const noexcept
    {
        return uniforms_;
    }

    // 事件处理
    void onMouseMove(float x, float y) noexcept
    {
        mousePos_ = glm::vec2(x, y);
    }

    void onMouseButton(bool pressed) noexcept
    {
        mouseDown_ = pressed;
        if (pressed)
        {
            mouseClickPos_ = mousePos_;
        }
    }

    void onResize(uint32_t width, uint32_t height) noexcept
    {
        uniforms_.iResolution = glm::vec3(width, height, 0.0F);
    }
};
// NOLINTEND