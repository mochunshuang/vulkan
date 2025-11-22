#pragma once

#include "constant.hpp"

// NOLINTBEGIN
class interval
{
  public:
    double min, max;

    constexpr interval() : min(+infinity), max(-infinity) {} // Default interval is empty

    constexpr interval(double min, double max) : min(min), max(max) {}

    // NOTE: aabb 相关：并集
    constexpr interval(const interval &a, const interval &b)
    {
        // 创建紧密包围两个输入区间的区间。
        min = a.min <= b.min ? a.min : b.min;
        max = a.max >= b.max ? a.max : b.max;
    }

    constexpr double size() const
    {
        return max - min;
    }

    constexpr bool contains(double x) const
    {
        return min <= x && x <= max;
    }

    constexpr bool surrounds(double x) const
    {
        return min < x && x < max;
    }

    constexpr double clamp(double x) const
    {
        if (x < min)
            return min;
        if (x > max)
            return max;
        return x;
    }

    // NOTE: AABB 相关。扩展区间
    interval expand(double delta) const
    {
        auto padding = delta / 2;
        return interval(min - padding, max + padding);
    }

    static const interval empty, universe;
};

const interval interval::empty = interval(+infinity, -infinity);
const interval interval::universe = interval(-infinity, +infinity);

interval operator+(const interval &ival, double displacement)
{
    return interval(ival.min + displacement, ival.max + displacement);
}

interval operator+(double displacement, const interval &ival)
{
    return ival + displacement;
}

// NOLINTEND