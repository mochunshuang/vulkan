#pragma once

#include "vec3.hpp"
#include "interval.hpp"
#include <ostream>

using color = vec3;

constexpr double linear_to_gamma(double linear_component)
{
    if (linear_component > 0)
        return std::sqrt(linear_component);

    return 0;
}

// 使用这个伽马校正，我们现在得到一个更一致的从黑暗到光明的斜坡：
constexpr void write_color(std::ostream &out, const color &pixel_color)
{
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

    // Apply a linear to gamma transform for gamma 2
    r = linear_to_gamma(r);
    g = linear_to_gamma(g);
    b = linear_to_gamma(b);

    constexpr auto k_mapping_value = 256;
    // Translate the [0,1] component values to the byte range [0,255].
    static const interval k_intensity(0.000, 0.999);
    int rbyte = static_cast<int>(k_mapping_value * k_intensity.clamp(r));
    int gbyte = static_cast<int>(k_mapping_value * k_intensity.clamp(g));
    int bbyte = static_cast<int>(k_mapping_value * k_intensity.clamp(b));

    // Write out the pixel color components.
    out << rbyte << ' ' << gbyte << ' ' << bbyte << '\n';
}