#pragma once

#include "color.hpp"
#include "hit_record.hpp"
#include "texture.hpp"

// NOLINTBEGIN
/*
产生散射光（或者说吸收了入射光）。
如果散射，说明光线应该衰减多少。
NOTE: 现在射线有了时间属性，我们需要更新 material::scatter() 来计算交集时间：
*/
class material
{
  public:
    virtual ~material() = default;

    // NOTE：scatter 分散的意思
    virtual bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                         ray &scattered) const
    {
        return false;
    }

    // NOTE: 发射的颜色。光源需要
    virtual color emitted(double u, double v, const point3 &p) const
    {
        return color(0, 0, 0);
    }
};

// NOTE: 反射建模。 反射的是材质的颜色。朗伯材料类
// NOTE: 为了支持过程纹理，我们将扩展lambertian类以使用纹理而不是颜色：
class lambertian : public material
{
  public:
    lambertian(const color &albedo) : tex(std::make_shared<solid_color>(albedo)) {}
    lambertian(const std::shared_ptr<vulkan_texture> &tex) : tex(tex) {}

    /*
    r_in：入射光线
    rec：击中记录（包含交点、法线等信息）
    attenuation：出参，光线衰减系数（颜色）
    scattered：出参，散射后的光线
    */
    bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                 ray &scattered) const override
    {
        auto scatter_direction = rec.normal + random_unit_vector();

        // 捕获零向量情况
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;

        // NOTE: ray 追加时间信息
        scattered = ray(rec.p, scatter_direction, r_in.time());

        // NOTE: 使用材料绑定的 纹理颜色
        attenuation = tex->value(rec.u, rec.v, rec.p);
        return true;
    }

  private:
    std::shared_ptr<vulkan_texture> tex;
};

class metal : public material
{
  public:
    metal(const color &albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                 ray &scattered) const override
    {
        // 计算入射光线在表面法线方向的理想反射方向。
        vec3 reflected = reflect(r_in.direction(), rec.normal);

        /*
        在理想反射方向基础上添加随机扰动：
            fuzz 越大，扰动越强，金属表面越粗糙
            fuzz = 0 时是完美镜面反射
            fuzz 被限制在 [0, 1] 范围内
        */
        reflected = unit_vector(reflected) + (fuzz * random_unit_vector());

        scattered = ray(rec.p, reflected, r_in.time());
        attenuation = albedo;

        // 确保散射方向与法线方向夹角小于90度（即不会反射到表面内部）
        return (dot(scattered.direction(), rec.normal) > 0);
    }

  private:
    color albedo;
    double fuzz;
};

// 一个电介质材质类，用于模拟透明材料（如玻璃、水等）的光线折射行为
class dielectric : public material
{
  public:
    dielectric(double refraction_index) : refraction_index(refraction_index) {}

    // 这是改进版的电介质散射函数，增加了全反射处理
    bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                 ray &scattered) const override
    {
        attenuation = color(1.0, 1.0, 1.0);
        double ri = rec.front_face ? (1.0 / refraction_index) : refraction_index;

        vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - (cos_theta * cos_theta));

        bool cannot_refract = ri * sin_theta > 1.0;
        vec3 direction;

        // NOTE: 还使用 Schlick 近似决定反射概率
        // NOTE: 随机数是实现基于物理的随机采样的关键。
        // 固定数，光线数量指数增长：每次交互都分裂成2条光线。经过几次反射后会有 2ⁿ 条光线
        if (cannot_refract || reflectance(cos_theta, ri) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        scattered = ray(rec.p, direction, r_in.time());
        return true;
    }

  private:
    // 折射率（在真空或空气中），或者材料的折射率与周围介质折射率的比值
    double refraction_index;

    static double reflectance(double cosine, double refraction_index)
    {
        // 使用Schlick的近似反射率: 廉价且有效
        auto r0 = (1 - refraction_index) / (1 + refraction_index);
        r0 = r0 * r0;
        return r0 + ((1 - r0) * std::pow((1 - cosine), 5));
    }
};

/*
NOTE: 发光材料
照明是光线追踪的关键组成部分。早期简单的光线跟踪器使用抽象光源，如空间中的点或方向。
现代方法有更多基于物理的光，它们有位置和大小。
为了创造这样的光源，我们需要能够把任何规则的物体变成向我们的场景发光的东西
*/
class diffuse_light : public material
{
  public:
    diffuse_light(std::shared_ptr<vulkan_texture> tex) : tex(tex) {}
    diffuse_light(const color &emit) : tex(std::make_shared<solid_color>(emit)) {}

    color emitted(double u, double v, const point3 &p) const override
    {
        return tex->value(u, v, p);
    }

  private:
    std::shared_ptr<vulkan_texture> tex;
};

/*
NOTE: 各向同性散射材质，专门用于体积渲染。
🎯 什么是各向同性散射？
各向同性 = 在所有方向上均匀散射，没有偏好方向。
对比其他散射类型：
  漫反射：主要在表面法线方向散射
  镜面反射：在反射方向散射
  各向同性：在所有360度方向均匀散射

*/
class isotropic : public material
{
  public:
    isotropic(const color &albedo) : tex(std::make_shared<solid_color>(albedo)) {}
    isotropic(std::shared_ptr<vulkan_texture> tex) : tex(tex) {}

    bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                 ray &scattered) const override
    {
        /*
        rec.p：散射发生的位置（在体积内部）
        random_unit_vector()：随机单位向量 - 这就是各向同性的核心！
        r_in.time()：保持光线时间一致性
        */
        scattered = ray(rec.p, random_unit_vector(), r_in.time());
        attenuation = tex->value(rec.u, rec.v, rec.p);
        return true;
    }

  private:
    std::shared_ptr<vulkan_texture> tex;
};
// NOLINTEND