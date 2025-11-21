#pragma once

#include "color.h"
#include "hit_record.h"

/*
产生散射光（或者说吸收了入射光）。
如果散射，说明光线应该衰减多少。
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
};

// NOTE: 反射建模。 反射的是材质的颜色。朗伯材料类
class lambertian : public material
{
  public:
    lambertian(const color &albedo) : albedo(albedo) {}

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

        scattered = ray(rec.p, scatter_direction);
        attenuation = albedo; // NOTE: 目前是固定的
        return true;
    }

  private:
    color albedo;
};

// 金属材料只是使用以下公式反射光线：
class metal_no_fuzz : public material
{
  public:
    explicit metal_no_fuzz(const color &albedo) : albedo(albedo) {}

    bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                 ray &scattered) const override
    {
        Vec3 reflected = reflect(r_in.direction(), rec.normal);
        scattered = ray(rec.p, reflected);
        attenuation = albedo;
        return true;
    }

  private:
    color albedo;
};

class metal : public material
{
  public:
    metal(const color &albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    /*
这种实现模拟了粗糙金属表面：
    完美光滑金属（fuzz=0）：严格的镜面反射
    粗糙金属（fuzz>0）：反射方向有随机偏差，产生模糊反射效果
    常用于模拟磨砂金属、陈旧金属表面等现实世界材质

这种模糊反射通过蒙特卡洛方法近似实现了微表面模型的效果。
*/
    bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                 ray &scattered) const override
    {
        // 计算入射光线在表面法线方向的理想反射方向。
        Vec3 reflected = reflect(r_in.direction(), rec.normal);

        /*
        在理想反射方向基础上添加随机扰动：
            fuzz 越大，扰动越强，金属表面越粗糙
            fuzz = 0 时是完美镜面反射
            fuzz 被限制在 [0, 1] 范围内
        */
        reflected = unit_vector(reflected) + (fuzz * random_unit_vector());

        // 创建从碰撞点出发的新光线，并设置衰减颜色为材质基色。
        scattered = ray(rec.p, reflected);
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
    explicit dielectric(double refraction_index) : refraction_index(refraction_index) {}

    bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                 ray &scattered) const override
    {
        attenuation = color(1.0, 1.0, 1.0); // 透明材料不吸收光，衰减为纯白色

        // 计算相对折射率：从外部进入时用1/ri，从内部射出时用ri
        double ri = rec.front_face ? (1.0 / refraction_index) : refraction_index;

        // 原理如下：根据折射率算出输出向量
        //  https://raytracing.github.io/images/fig-1.17-refraction.jpg
        Vec3 unit_direction = unit_vector(r_in.direction());
        Vec3 refracted = refract(unit_direction, rec.normal, ri); // 计算折射方向

        scattered = ray(rec.p, refracted); // 创建折射光线
        return true;                       // 总是发生散射（折射）
    }

  private:
    // 折射率（在真空或空气中），或者材料的折射率与周围介质折射率的比值
    double refraction_index;
};

/*
这个实现模拟了真实透明材料的两种光学现象：
    正常折射 - 光线穿过界面时改变方向
    全反射 - 当光线从光密介质射向光疏介质且入射角大于临界角时，全部反射回原介质

典型应用：
    玻璃棱镜中的全反射
    光纤通信的光传输
    水下看水面时的镜面效果

这个版本比之前的实现更物理准确，考虑了全反射这一重要光学现象
*/
class dielectric_full_internal_reflection : public material
{
  public:
    explicit dielectric_full_internal_reflection(double refraction_index)
        : refraction_index(refraction_index)
    {
    }

    // 这是改进版的电介质散射函数，增加了全反射处理
    bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                 ray &scattered) const override
    {
        /*

根据斯涅尔定律：n₁sinθ₁ = n₂sinθ₂
    当 ri * sin_theta > 1.0 时，sinθ₂ > 1，这在物理上不可能
    此时发生全反射（Total Internal Reflection）

cos_theta：入射光线与法线夹角的余弦值
sin_theta：通过 sin²θ + cos²θ = 1 计算得出

*/
        attenuation = color(1.0, 1.0, 1.0); // 透明材料不吸收光，衰减为纯白色

        // 计算相对折射率：从外部进入时用1/ri，从内部射出时用ri
        double ri = rec.front_face ? (1.0 / refraction_index) : refraction_index;

        Vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta =
            std::fmin(dot(-unit_direction, rec.normal), 1.0);        // 入射角余弦值
        double sin_theta = std::sqrt(1.0 - (cos_theta * cos_theta)); // 入射角正弦值

        // 检查是否发生全反射：当 ri * sin_theta > 1.0 时无法折射
        bool cannot_refract = ri * sin_theta > 1.0;
        Vec3 direction;

        if (cannot_refract)
            direction = reflect(unit_direction, rec.normal); // 全反射：使用反射
        else
            direction = refract(unit_direction, rec.normal, ri); // 正常折射

        scattered = ray(rec.p, direction);
        return true; // 总是发生散射（折射或反射）
    }

  private:
    // 折射率（在真空或空气中），或者材料的折射率与周围介质折射率的比值
    double refraction_index;
};

/*
为什么使用 Schlick 近似？
计算效率：比精确的菲涅尔方程快很多
物理合理性：在大多数情况下与真实物理结果非常接近
广泛应用：成为图形学中的标准方法

物理意义
Schlick 近似捕捉了菲涅尔效应的关键特征：
    在掠射角（grazing angle）时反射率接近 1
    在法线入射时反射率最小
    平滑过渡 between 这两种极端情况

这使得渲染的玻璃、水面等电介质材料看起来更加真实
*/
class dielectric_schlick : public material
{
  public:
    explicit dielectric_schlick(double refraction_index)
        : refraction_index(refraction_index)
    {
    }

    bool scatter(const ray &r_in, const hit_record &rec, color &attenuation,
                 ray &scattered) const override
    {
        attenuation = color(1.0, 1.0, 1.0);
        double ri = rec.front_face ? (1.0 / refraction_index) : refraction_index;

        Vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - (cos_theta * cos_theta));

        bool cannot_refract = ri * sin_theta > 1.0;
        Vec3 direction;

        /*
在真实物理中，当光线到达电介质界面时：
    一部分光线会反射
    另一部分光线会折射

这个比例由入射角和材质折射率决定
*/
        // 使用 Schlick 近似决定反射概率
        // NOTE: 随机数是实现基于物理的随机采样的关键。
        // 固定数，光线数量指数增长：每次交互都分裂成2条光线。经过几次反射后会有 2ⁿ 条光线
        if (cannot_refract || reflectance(cos_theta, ri) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        scattered = ray(rec.p, direction);
        return true;
    }

  private:
    // Refractive index in vacuum or air, or the ratio of the material's refractive index
    // over the refractive index of the enclosing media
    double refraction_index;

    static double reflectance(double cosine, double refraction_index)
    {
        // 使用Schlick的近似反射率: 廉价且有效
        auto r0 = (1 - refraction_index) / (1 + refraction_index);
        r0 = r0 * r0;
        return r0 + ((1 - r0) * std::pow((1 - cosine), 5));
    }
};