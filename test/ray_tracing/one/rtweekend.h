#pragma once

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <fstream>
#include <random>

// C++ Std Usings

// using std::make_shared;
// using std::shared_ptr;

// Constants

#include "interval.h"

// Utility Functions

inline double degrees_to_radians(double degrees)
{
    return degrees * pi / 180.0;
}

// Common Headers

#include "color.h"
#include "ray.h"
#include "vec3.h"