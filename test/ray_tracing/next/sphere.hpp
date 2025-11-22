#pragma once

#include <utility>

#include "hittable.hpp"
#include "vec3.hpp"

class sphere : public hittable // NOLINT
{
  public:
    // Stationary Sphere
    sphere(const point3 &static_center, double radius, std::shared_ptr<material> mat)
        : center_(static_center, vec3(0, 0, 0)), radius_(std::fmax(0, radius)),
          mat_(std::move(mat))
    {
        auto rvec = vec3(radius, radius, radius);
        bbox_ = aabb(static_center - rvec, static_center + rvec);
    }

    // Moving Sphere
    sphere(const point3 &center1, const point3 &center2, double radius,
           std::shared_ptr<material> mat)
        : center_(center1, center2 - center1), radius_(std::fmax(0, radius)),
          mat_(std::move(mat))
    {
        auto rvec = vec3(radius, radius, radius);
        aabb box1(center_.at(0) - rvec, center_.at(0) + rvec);
        aabb box2(center_.at(1) - rvec, center_.at(1) + rvec);
        bbox_ = aabb(box1, box2);
    }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override
    {
        // NOTE: 需要从 射线中，获得中心点 才能兼容原本的代码
        point3 current_center = center_.at(r.time()); // NOTE: 运动中心的位置，时间确定
        vec3 oc = current_center - r.origin();

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

        vec3 outward_normal = (rec.p - current_center) / radius_;
        rec.set_face_normal(r, outward_normal);

        rec.mat = mat_;

        // NOTE: 填写球体 u,v 的坐标
        get_sphere_uv(outward_normal, rec.u, rec.v);

        return true;
    }

    [[nodiscard]] aabb bounding_box() const override
    {
        return bbox_;
    }

  private:
    ray center_; // NOTE: 1. 运动模糊需要让 点 变成射线类
    double radius_;

    // 带有添加材料信息的Ray球体交集
    std::shared_ptr<material> mat_;

    aabb bbox_;

    static void get_sphere_uv(const point3 &p, double &u, double &v)
    {
        // p: a given point on the sphere of radius one, centered at the origin.
        // u: returned value [0,1] of angle around the Y axis from X=-1.
        // v: returned value [0,1] of angle from Y=-1 to Y=+1.
        //     <1 0 0> yields <0.50 0.50>       <-1  0  0> yields <0.00 0.50>
        //     <0 1 0> yields <0.50 1.00>       < 0 -1  0> yields <0.50 0.00>
        //     <0 0 1> yields <0.25 0.50>       < 0  0 -1> yields <0.75 0.50>

        auto theta = std::acos(-p.y());
        auto phi = std::atan2(-p.z(), p.x()) + pi;

        u = phi / (2 * pi);
        v = theta / pi;
    }
};
