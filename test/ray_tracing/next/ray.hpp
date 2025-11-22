#pragma once

#include "vec3.hpp"

// NOLINTBEGIN
class ray
{
  public:
    ray() = default;

    constexpr ray(const point3 &origin, const vec3 &direction, double time)
        : orig(origin), dir(direction), tm(time)
    {
    }

    constexpr ray(const point3 &origin, const vec3 &direction) : ray(origin, direction, 0)
    {
    }

    [[nodiscard]] constexpr const point3 &origin() const
    {
        return orig;
    }
    [[nodiscard]] constexpr const vec3 &direction() const
    {
        return dir;
    }

    /*
    在真实的相机中，快门保持打开的时间间隔很短，在此期间，世界上的相机和物体可能会移动。
    NOTE: 增加时间参数，来模拟 相机捕捉的  运动模糊
    */
    [[nodiscard]] constexpr double time() const
    {
        return tm;
    }

    [[nodiscard]] constexpr point3 at(double t) const
    {
        return orig + t * dir;
    }

  private:
    point3 orig;
    vec3 dir;

    /*
快门计时有两个方面需要考虑：从一个快门打开到下一个快门打开的时间，以及每帧快门保持打开的时间。标准电影胶片过去以每秒24帧的速度拍摄。
现代数字电影可以是24、30、48、60、120帧，或者导演想要的每秒多少帧。
每一帧都可以有自己的快门速度。这个快门速度不需要——通常也不是——整个帧的最大持续时间。你可以每帧打开快门1/1000秒，或者1/60秒。

如果你想渲染一系列图像，你需要用适当的快门时间来设置相机：帧到帧周期、快门/渲染持续时间和总帧数（总拍摄时间）。
如果相机在移动，世界是静态的，你就可以走了。
但是，如果世界上有任何东西在移动，你需要向hittable添加一个方法，这样每个对象都可以知道当前帧的时间段

*/
    double tm;
}; // NOLINTEND