#pragma once

#include <memory>
#include "ray.hpp"
#include "vec3.hpp"

class material;

// NOLINTBEGIN
struct hit_record
{
    point3 p;    // 交点位置
    vec3 normal; // 法向量
    double t;    // 光线参数

    std::shared_ptr<material> mat; // 材质

    bool front_face; // 正面还是背面

    // 更新hit_record结构来存储u，v.
    // NOTE: 纹理相关：光线物体命中点的表面坐标
    double u;
    double v;

    constexpr void set_face_normal(const ray &r, const vec3 &outward_normal)
    {
        // NOTE: 假设参数'outard_normal'具有单位长度。
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
}; // NOLINTEND