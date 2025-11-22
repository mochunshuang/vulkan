#pragma once

#include "material.hpp"

#include "color.hpp"
#include "degrees_to_radians.hpp"
#include "hittable.hpp"

#include <iostream>

// NOLINTBEGIN
class camera
{
  public:
    double aspect_ratio = 1.0;  // 图像宽高比（宽度/高度）
    int image_width = 100;      // 渲染图像的像素宽度
    int samples_per_pixel = 10; // 每个像素的随机采样次数
    int max_depth = 10;         // 光线在场景中的最大反弹次数

    double vfov = 90; // 垂直视野角度（视场角）

    point3 lookfrom = point3(0, 0, 0); // 相机位置（观察起点）
    point3 lookat = point3(0, 0, -1);  // 相机观察目标点
    vec3 vup = vec3(0, 1, 0);          // 相机上方向向量

    double defocus_angle = 0; // 通过每个像素的光线变化角度（景深效果）
    double focus_dist = 10;   // 相机到完美对焦平面的距离

    // NOTE: 背景颜色，可以是黑的，这样光源就只能由我们自己定义了
    color background; // Scene background color

    void render(const hittable &world, std::ostream &out)
    {
        // NOTE: 禁用同步
        std::ostream::sync_with_stdio(false);

        initialize(); // 初始化相机参数

        out << "P3\n" << image_width << ' ' << imageHeight_ << "\n255\n";

        for (int j = 0; j < imageHeight_; j++)
        {
            std::clog << "\rScanlines remaining: " << (imageHeight_ - j) << ' '
                      << std::flush;
            for (int i = 0; i < image_width; i++)
            {
                color pixel_color(0, 0, 0);
                // 对每个像素进行多次采样（抗锯齿）
                for (int sample = 0; sample < samples_per_pixel; sample++)
                {
                    ray r = get_ray(i, j); // 获取通过像素(i,j)的光线
                    pixel_color += ray_color(r, max_depth, world); // 计算光线颜色
                }
                write_color(out,
                            pixelSamplesScale_ * pixel_color); // 输出像素颜色
            }
        }

        std::cout << "\rDone.                 \n";
    }
    void render_with_background(const hittable &world, std::ostream &out)
    {
        // NOTE: 禁用同步
        std::ostream::sync_with_stdio(false);

        initialize(); // 初始化相机参数

        out << "P3\n" << image_width << ' ' << imageHeight_ << "\n255\n";

        for (int j = 0; j < imageHeight_; j++)
        {
            std::clog << "\rScanlines remaining: " << (imageHeight_ - j) << ' '
                      << std::flush;
            for (int i = 0; i < image_width; i++)
            {
                color pixel_color(0, 0, 0);
                // 对每个像素进行多次采样（抗锯齿）
                for (int sample = 0; sample < samples_per_pixel; sample++)
                {
                    ray r = get_ray(i, j); // 获取通过像素(i,j)的光线
                    pixel_color +=
                        ray_color_with_background(r, max_depth, world); // 计算光线颜色
                }
                write_color(out,
                            pixelSamplesScale_ * pixel_color); // 输出像素颜色
            }
        }

        std::cout << "\rDone.                 \n";
    }

  private:
    int imageHeight_;          // 渲染图像的像素高度
    double pixelSamplesScale_; // 像素采样总和的颜色缩放因子
    point3 center_;            // 相机中心位置
    point3 pixel00Loc_;        // 像素(0,0)的位置
    vec3 pixelDeltaU_;         // 向右相邻像素的偏移量
    vec3 pixelDeltaV_;         // 向下相邻像素的偏移量

    // NOTE: 相机坐标系
    vec3 u_, v_, w_; // 相机坐标系的基向量

    // NOTE: 聚焦，模拟物理相机
    vec3 defocusDiskU_; // 散焦圆盘水平半径
    vec3 defocusDiskV_; // 散焦圆盘垂直半径

    void initialize()
    {
        // NOTE:0. 基本信息
        imageHeight_ = static_cast<int>(image_width / aspect_ratio);
        imageHeight_ = (imageHeight_ < 1) ? 1 : imageHeight_;

        pixelSamplesScale_ = 1.0 / samples_per_pixel; // 计算采样缩放因子

        // NOTE:1. vfov: z 和 h 是有关系的。确定z旧确定h
        auto theta = degrees_to_radians(vfov); // 将角度转换为弧度
        auto h = std::tan(theta / 2);

        // NOTE:2. lookfrom,lookat
        center_ = lookfrom;
        w_ = unit_vector(lookfrom - lookat); // 前向向量
        u_ = unit_vector(cross(vup, w_));    // 右向向量
        v_ = cross(w_, u_);                  // 上向向量

        // NOTE: 3. 在视口启动聚焦。而不是直接中心距离
        auto viewport_height = 2 * h * focus_dist; // 视口高度

        auto viewport_width = viewport_height * (static_cast<double>(image_width) /
                                                 imageHeight_); // 视口宽度

        vec3 viewport_u = viewport_width * u_;   // 视口水平边缘向量
        vec3 viewport_v = viewport_height * -v_; // 视口垂直边缘向量（向下）

        // 计算像素到像素的水平垂直增量向量
        pixelDeltaU_ = viewport_u / image_width;
        pixelDeltaV_ = viewport_v / imageHeight_;

        // NOTE:3. 修改视口，因聚焦盘
        auto viewport_upper_left =
            center_ - (focus_dist * w_) - viewport_u / 2 - viewport_v / 2;
        pixel00Loc_ = viewport_upper_left + 0.5 * (pixelDeltaU_ + pixelDeltaV_);

        // NOTE:3. 计算相机散焦圆盘基向量
        auto defocus_radius =
            focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocusDiskU_ = u_ * defocus_radius;
        defocusDiskV_ = v_ * defocus_radius;
    }

    // 构建从散焦圆盘发出并指向像素(i,j)周围随机采样点的相机光线
    [[nodiscard]] ray get_ray(int i, int j) const
    {
        auto offset = sample_square(); // 获取方形区域内的随机偏移
        auto pixel_sample = pixel00Loc_ + ((i + offset.x()) * pixelDeltaU_) +
                            ((j + offset.y()) * pixelDeltaV_); // 计算像素采样点

        // NOTE: 3. 光线可以来自聚焦盘
        auto ray_origin =
            (defocus_angle <= 0) ? center_ : defocus_disk_sample(); // 光线起点
        auto ray_direction = pixel_sample - ray_origin;             // 光线方向

        // NOTE:4. 模拟运动模糊，需要 ray 带时间信息
        auto ray_time = random_double();
        return ray(ray_origin, ray_direction, ray_time);
    }

    [[nodiscard]] vec3 sample_square() const
    {
        // 返回[-0.5,-0.5]到[+0.5,+0.5]单位方形区域内的随机点向量
        return {random_double() - 0.5, random_double() - 0.5, 0};
    }

    [[nodiscard]] vec3 sample_disk(double radius) const
    {
        // 返回以原点为中心、指定半径的圆盘内的随机点
        return radius * random_in_unit_disk();
    }

    [[nodiscard]] point3 defocus_disk_sample() const
    {
        // 返回相机散焦圆盘内的随机点
        auto p = random_in_unit_disk();
        return center_ + (p[0] * defocusDiskU_) + (p[1] * defocusDiskV_);
    }

    [[nodiscard]] color ray_color(const ray &r, int depth, const hittable &world) const
    {
        // 如果达到光线反弹次数限制，不再收集光线
        if (depth <= 0)
            return {0, 0, 0};

        hit_record rec;

        // 检测光线是否与场景中的物体相交
        if (world.hit(r, interval(0.001, infinity), rec))
        {
            ray scattered;
            color attenuation;
            // 如果材质散射光线，递归计算散射光线颜色
            if (rec.mat->scatter(r, rec, attenuation, scattered))
                return attenuation * ray_color(scattered, depth - 1, world);
            return {0, 0, 0}; // 完全吸收
        }

        // 如果没有击中物体，渲染背景色（天空盒）
        vec3 unit_direction = unit_vector(r.direction());
        auto a = 0.5 * (unit_direction.y() + 1.0); // 计算垂直方向的混合因子
        // 线性混合：白色（顶部）到蓝色（底部）
        return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
    }

    [[nodiscard]] color ray_color_with_background(const ray &r, int depth,
                                                  const hittable &world) const
    {
        // 如果达到光线反弹次数限制，不再收集光线
        if (depth <= 0)
            return {0, 0, 0};

        hit_record rec;

        // If the ray hits nothing, return the background color.
        if (not world.hit(r, interval(0.001, infinity), rec))
            return background;

        ray scattered;
        color attenuation;
        color color_from_emission = rec.mat->emitted(rec.u, rec.v, rec.p);

        if (!rec.mat->scatter(r, rec, attenuation, scattered))
            return color_from_emission;

        color color_from_scatter =
            attenuation * ray_color_with_background(scattered, depth - 1, world);

        return color_from_emission + color_from_scatter;
    }
};
// NOLINTEND