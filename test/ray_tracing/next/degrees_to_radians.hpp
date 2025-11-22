#pragma once

#include "constant.hpp"

constexpr double degrees_to_radians(double degrees) noexcept
{
    return degrees * pi / 180.0; // NOLINT
}