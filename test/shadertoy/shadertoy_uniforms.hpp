#pragma once

#include <glm/glm.hpp>
#include <array>

// NOLINTBEGIN
// https://www.shadertoy.com/view/Xds3zN
struct shadertoy_uniforms
{
    alignas(16) glm::vec3 iResolution;             // viewport resolution (in pixels)
    alignas(4) float iTime;                        // shader playback time (in seconds)
    alignas(4) float iTimeDelta;                   // render time (in seconds)
    alignas(4) float iFrameRate;                   // shader frame rate
    alignas(4) int iFrame;                         // shader playback frame
    alignas(16) std::array<float, 4> iChannelTime; // channel playback time
    alignas(16) std::array<glm::vec3, 4> iChannelResolution; // channel resolution
    alignas(16) glm::vec4 iMouse;                            // mouse pixel coords
    alignas(16) glm::vec4 iDate;  // (year, month, day, time in seconds)
    alignas(4) float iSampleRate; // sound sample rate

    // 构造函数
    shadertoy_uniforms()
        : iResolution(0, 0, 0), iTime(0.0f), iTimeDelta(0.016f) // 60 FPS默认值
          ,
          iChannelTime{}, iChannelResolution{}, iFrameRate(60.0f), iFrame(0),
          iMouse(0, 0, 0, 0), iDate(0, 0, 0, 0), iSampleRate(44100.0f)
    {
    }
};
// NOLINTEND
