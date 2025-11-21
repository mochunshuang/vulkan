#pragma once

#include <utility>

#include "hittable.h"
#include "vec3.h"

class sphere : public hittable
{
  public:
    sphere(const point3 &center, double radius, std::shared_ptr<material> mat = nullptr)
        : center_(center), radius_(std::fmax(0, radius)), mat_(std::move(mat))

    {
        // std::fmax 是 C++ 标准库中的一个数学函数，用于比较两个浮点数并返回较大的那个值。
    }
    /*
    NOTE: 在真实的光线追踪中，光线有起点和终点范围：
        ray_tmin: 最小有效距离（避免自相交）
        ray_tmax: 最大有效距离（避免穿过物体）
    */
    /*
光线方向 →
                     ↓
                     │
                     │          球体边界
                     │         ↙     ↘
光线起点 o━━━━━━━━━━━━━┼━━━━━━━●━━━━━━━●━━━━━━━━━━━━━━→
(t=0)               │       t1      t2
                     │      (0.5)   (1.5)
                     │
有效检测范围:       [0.001 ━━━━━━━━━━━━━━━ 1000.0]
                     ↑                  ↑
                 ray_tmin           ray_tmax

检测逻辑:
1. 检查 t1=0.5: 在 [0.001, 1000] 范围内 ✓ → 选择t1
2. 如果 t1 无效，检查 t2=1.5
3. 如果都无效，返回 false

t1有效：
[0.001 ━━━●━━━━━━━━━━━━━━ 1000.0]
         t1=0.5
        ✓ 选择t1

t1无效，t2有效：
[0.001 ━━━━━━━━━●━━━━━━━━ 1000.0]
               t2=1.5
              ✓ 选择t2

两个交点都无效：
[0.001 ━━━━━━━━━━━━━━━ 1000.0] ●━━●
                             t1=1001 t2=1002
                             ✗ 都超出范围
*/
    bool hit(const ray &r, double ray_tmin, double ray_tmax,
             hit_record &rec) const override
    {
        Vec3 oc = center_ - r.origin();
        auto a = r.direction().length_squared();
        auto h = dot(r.direction(), oc);
        auto c = oc.length_squared() - (radius_ * radius_);

        auto discriminant = (h * h) - (a * c); // 判别式：h² - a*c
        if (discriminant < 0)
            return false; // NOTE: 射线和球没有交点

        // NOTE: 开根号
        auto sqrtd = std::sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range.
        // 先检查近交点 t1
        auto root = (h - sqrtd) / a;
        if (root <= ray_tmin || ray_tmax <= root)
        {
            // 近交点无效，检查远交点 t2
            root = (h + sqrtd) / a;
            if (root <= ray_tmin || ray_tmax <= root)
                return false; // 两个交点都无效
        }

        rec.t = root;
        rec.p = r.at(rec.t); // NOTE:交点位置

        // NOTE: 总是从球心指向外部
        // rec.normal = (rec.p - center_) / radius_; // NOTE: 单位法向量

        // 核心逻辑：通过点积判断光线在物体的哪一侧
        //  NOTE: 点积判断方向下用处
        //   点积判断原理：
        //   ray · normal > 0：光线与法线同向 → 在物体内部
        //   ray · normal < 0：光线与法线反向 → 在物体外部
        Vec3 outward_normal = (rec.p - center_) / radius_;
        rec.set_face_normal(r, outward_normal); // 设置正面/背面信息

        return true;
    }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override
    {
        Vec3 oc = center_ - r.origin();
        auto a = r.direction().length_squared();
        auto h = dot(r.direction(), oc);
        auto c = oc.length_squared() - (radius_ * radius_);

        auto discriminant = (h * h) - (a * c);
        if (discriminant < 0)
            return false;

        auto sqrtd = std::sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range.
        auto root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root))
        {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root))
                return false;
        }
        rec.t = root;
        rec.p = r.at(rec.t);
        Vec3 outward_normal = (rec.p - center_) / radius_;
        rec.set_face_normal(r, outward_normal);

        rec.mat = mat_;

        return true;
    }

  private:
    point3 center_;
    double radius_;

    // 带有添加材料信息的Ray球体交集
    std::shared_ptr<material> mat_;
};
