#pragma once

#include "interval.h"
#include "vec3.h"
#include <iostream>

using color = Vec3;

constexpr void write_color(std::ostream &out, const color &pixel_color)
{

#if 0
    auto x = pixel_color.x();
    auto y = pixel_color.y();
    auto z = pixel_color.z();
    constexpr auto k_mapping_value = 255.999;
    // Translate the [0,1] component values to the byte range [0,255].
    int rbyte = static_cast<int>(k_mapping_value * x);
    int gbyte = static_cast<int>(k_mapping_value * y);
    int bbyte = static_cast<int>(k_mapping_value * z);
#else
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();
    constexpr auto k_mapping_value = 256;
    // Translate the [0,1] component values to the byte range [0,255].
    static const interval k_intensity(0.000, 0.999);
    int rbyte = static_cast<int>(k_mapping_value * k_intensity.clamp(r));
    int gbyte = static_cast<int>(k_mapping_value * k_intensity.clamp(g));
    int bbyte = static_cast<int>(k_mapping_value * k_intensity.clamp(b));
#endif

    // Write out the pixel color components.
    out << rbyte << ' ' << gbyte << ' ' << bbyte << '\n';
}

inline double linear_to_gamma(double linear_component)
{
    if (linear_component > 0)
        return std::sqrt(linear_component);

    return 0;
}
// 使用这个伽马校正，我们现在得到一个更一致的从黑暗到光明的斜坡：
void write_color_with_gamma(std::ostream &out, const color &pixel_color)
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
