#pragma once

#include "vec3.h"

// NOLINTBEGIN
class ray
{
  public:
    ray() = default;

    ray(const point3 &origin, const Vec3 &direction) : orig(origin), dir(direction) {}

    [[nodiscard]] const point3 &origin() const
    {
        return orig;
    }
    [[nodiscard]] const Vec3 &direction() const
    {
        return dir;
    }

    [[nodiscard]] point3 at(double t) const
    {
        /*
    P(t) = A + tb
这是一个参数化直线方程，在光线追踪中具有核心意义：
    A：光线的起点（射线原点），比如相机位置
    b：光线的方向向量（单位向量）
    t：沿着光线前进的距离参数
    P(t)：在距离 t 处的位置点

想象你站在点 A，朝着方向 b 看：
    t = 0：你站的位置 (A)
    t = 1：你面前1米的位置
    t = 2：你面前2米的位置
    t = -1：你身后1米的位置

目的:
    // 对于每个像素，发射一条光线
    ray r(camera_center, direction_to_pixel);

    // 沿着光线检测与物体的交点
    for (const auto& object : scene_objects) {
        if (object.hit(r, 0, INFINITY)) {  // 检测 t 在 [0, ∞) 范围内的交点
            // 这个物体被光线击中了！
            color = calculate_color(object);
        }
    }
         */
        return orig + t * dir;
    }

  private:
    point3 orig;
    Vec3 dir;
};
// NOLINTEND