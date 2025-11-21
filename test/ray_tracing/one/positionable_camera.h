#pragma once

#include "color.h"
#include "hittable.h"
#include "material.h"
#include "rtweekend.h"
#include <iostream>

class positionable_camera
{
  public:
    double aspect_ratio = 1.0;  // 图像宽高比（宽度/高度）
    int image_width = 100;      // 渲染图像的像素宽度
    int samples_per_pixel = 10; // 每个像素的随机采样次数
    int max_depth = 10;         // 光线在场景中的最大反弹次数

    /*
NOTE: 首先，让我们允许可调视野（fov）。
这是渲染图像从边缘到边缘的可视角度。
NOTE: 由于我们的图像不是方形的，所以fov在水平和垂直方向上是不同的。我总是使用垂直fov。
我通常也会在构造函数中以度指定它并更改为弧度——这是个人品味的问题

https://raytracing.github.io/images/fig-1.18-cam-view-geom.jpg
*/
    double vfov = 90; // 垂直视野角度（视场角）

    /*
要获得任意视角，我们首先命名相关的点。我们将相机放置的位置称为
lookfrom（观察起点），将我们观察的点称为
lookat（观察目标点）。（之后，如果你愿意，可以定义观察方向而不是观察目标点。）
NOTE: 我们还需要一种方法来指定相机的滚动或侧向倾斜，即围绕 lookat-lookfrom轴的旋转。

另一种理解方式是：即使保持 lookfrom 和 lookat不变，你仍然可以围绕鼻子旋转头部。
我们需要的是指定相机"上"向量的方法。

https://raytracing.github.io/images/fig-1.19-cam-view-dir.jpg
https://raytracing.github.io/images/fig-1.20-cam-view-up.jpg
*/
    point3 lookfrom = point3(0, 0, 0); // 相机位置（观察起点）
    point3 lookat = point3(0, 0, -1);  // 相机观察目标点
    Vec3 vup = Vec3(0, 1, 0);          // 相机上方向向量

    /*
核心概念:
    散焦模糊 = 景深效果（在摄影中称为 Depth of Field）

物理原理:
    真实相机需要大孔洞（而非针孔）来收集足够的光线
    大孔洞会导致模糊，但通过镜头可以控制对焦平面
    镜头作用：将对焦距离上某点的所有光线重新聚焦到传感器上的单个点

关键参数
    对焦距离（Focus Distance）：相机中心到完美对焦平面的距离
        在此距离的物体最清晰
        离此距离越远，模糊程度越大

    焦距（Focal Length）：相机中心到图像平面的距离
        在我们的模型中，对焦距离 = 焦距

相机工作机制
    对焦控制：通过调节镜头与传感器之间的距离来改变对焦距离

    光圈（Aperture）：控制镜头有效大小的孔洞
        大光圈 → 更多光线，但离焦物体更模糊
        小光圈 → 较少光线，但景深更大

虚拟相机的特殊考虑
    拥有完美传感器，永远不需要更多光线
    NOTE: 只在使用散焦模糊时才需要模拟光圈
    主要目的是艺术效果，而非光线收集

重要区别
对摄影师说：景深（Depth of Field）
对光线追踪朋友说：散焦模糊（Defocus Blur）

NOTE: 这个功能让渲染结果更加真实，模拟了真实相机镜头的物理特性。
*/
    /*
真正的相机有一个复杂的复合镜头。对于我们的代码，我们可以模拟顺序：传感器，然后镜头，然后光圈。然后我们可以弄清楚将光线发送到哪里，并在计算后翻转图像（图像被颠倒投影在胶片上）。
然而，图形人员通常使用薄镜头近似：
在实践中，我们通过将视口放在这个平面中来实现这一点。把所有东西放在一起：
    聚焦平面与相机视图方向正交。
    对焦距离是相机中心与对焦平面之间的距离。
    视口位于焦点平面上，以相机视图方向向量为中心。
    像素位置的网格位于视口内（位于3D世界中）。
    从当前像素位置周围的区域中选择随机图像样本位置。
    相机通过当前图像样本位置从镜头上的随机点发射光线。

https://raytracing.github.io/images/fig-1.21-cam-lens.jpg
https://raytracing.github.io/images/fig-1.22-cam-film-plane.jpg


没有散焦模糊，所有场景光线都源自相机中心（或lookfrom）。
为了完成散焦模糊，我们构造了一个以相机中心为中心的圆盘。半径越大，散焦模糊越大。
你可以认为我们原来的相机有一个半径为零的散焦圆盘（根本没有模糊），所以所有光线都起源于圆盘中心（lookfrom）。
    因为我们将从散焦盘中选择随机点，所以我们需要一个函数来做到这一点：random_in_unit_disk（）。
    这个函数使用我们在random_unit_vector（）中使用的相同方法工作，只是用于二维。
*/
    double defocus_angle = 0; // 通过每个像素的光线变化角度（景深效果）
    double focus_dist = 10;   // 相机到完美对焦平面的距离

    void render(const hittable &world, std::ostream &out)
    {
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
                write_color_with_gamma(out,
                                       pixelSamplesScale_ * pixel_color); // 输出像素颜色
            }
        }

        std::cout << "\rDone.                 \n";
    }
    void render_focus(const hittable &world, std::ostream &out)
    {
        // NOTE: 禁用同步
        std::ostream::sync_with_stdio(false);

        initialize_focus(); // 初始化相机参数

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
                    ray r = get_ray_focus(i, j); // 获取通过像素(i,j)的光线
                    pixel_color += ray_color(r, max_depth, world); // 计算光线颜色
                }
                write_color_with_gamma(out,
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
    Vec3 pixelDeltaU_;         // 向右相邻像素的偏移量
    Vec3 pixelDeltaV_;         // 向下相邻像素的偏移量

    // NOTE: 相机坐标系
    Vec3 u_, v_, w_; // 相机坐标系的基向量

    // NOTE: 聚焦，模拟物理相机
    Vec3 defocusDiskU_; // 散焦圆盘水平半径
    Vec3 defocusDiskV_; // 散焦圆盘垂直半径

    void initialize()
    {
        // 根据宽高比计算图像高度
        imageHeight_ = static_cast<int>(image_width / aspect_ratio);
        imageHeight_ = (imageHeight_ < 1) ? 1 : imageHeight_;

        pixelSamplesScale_ = 1.0 / samples_per_pixel; // 计算采样缩放因子

        // NOTE:1. vfov: z 和 h 是有关系的。确定z旧确定h
        //  https://raytracing.github.io/images/fig-1.18-cam-view-geom.jpg
        // 确定视口尺寸.下面三行的固定的
        auto theta = degrees_to_radians(vfov); // 将角度转换为弧度
        auto h = std::tan(theta / 2);

        auto focal_length = (lookfrom - lookat).length();
        auto viewport_height = 2 * h * focal_length;
        auto viewport_width = viewport_height * (static_cast<double>(image_width) /
                                                 imageHeight_); // 视口宽度

        // NOTE:2.0 lookfrom 和 lookat 轴
        center_ = lookfrom; // 设置相机中心位置
        // NOTE:2.1 计算相机坐标系的u,v,w单位基向量
        w_ = unit_vector(lookfrom - lookat); // 前向向量
        u_ = unit_vector(cross(vup, w_));    // 右向向量
        v_ = cross(w_, u_);                  // 上向向量

        // NOTE:2.2 计算横跨视口水平和垂直边缘的向量
        Vec3 viewport_u = viewport_width * u_;   // 视口水平边缘向量
        Vec3 viewport_v = viewport_height * -v_; // 视口垂直边缘向量（向下）

        // 计算像素到像素的水平垂直增量向量
        pixelDeltaU_ = viewport_u / image_width;
        pixelDeltaV_ = viewport_v / imageHeight_;

        // NOTE:2.3 计算左上角像素的位置
        auto viewport_upper_left =
            center_ - (focal_length * w_) - viewport_u / 2 - viewport_v / 2;
        pixel00Loc_ = viewport_upper_left + 0.5 * (pixelDeltaU_ + pixelDeltaV_);
    }

    void initialize_focus()
    {
        // 根据宽高比计算图像高度
        imageHeight_ = static_cast<int>(image_width / aspect_ratio);
        imageHeight_ = (imageHeight_ < 1) ? 1 : imageHeight_;

        pixelSamplesScale_ = 1.0 / samples_per_pixel; // 计算采样缩放因子

        auto theta = degrees_to_radians(vfov); // 将角度转换为弧度
        auto h = std::tan(theta / 2);

        // NOTE: 3. 在视口启动聚焦。而不是直接中心距离
        auto viewport_height = 2 * h * focus_dist; // 视口高度

        auto viewport_width = viewport_height * (static_cast<double>(image_width) /
                                                 imageHeight_); // 视口宽度

        center_ = lookfrom;
        w_ = unit_vector(lookfrom - lookat); // 前向向量
        u_ = unit_vector(cross(vup, w_));    // 右向向量
        v_ = cross(w_, u_);                  // 上向向量

        Vec3 viewport_u = viewport_width * u_;   // 视口水平边缘向量
        Vec3 viewport_v = viewport_height * -v_; // 视口垂直边缘向量（向下）

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

    [[nodiscard]] ray get_ray(int i, int j) const
    {
        // 构建从散焦圆盘发出并指向像素(i,j)周围随机采样点的相机光线

        auto offset = sample_square(); // 获取方形区域内的随机偏移
        auto pixel_sample = pixel00Loc_ + ((i + offset.x()) * pixelDeltaU_) +
                            ((j + offset.y()) * pixelDeltaV_); // 计算像素采样点

        auto ray_origin = center_;                      // 光线起点
        auto ray_direction = pixel_sample - ray_origin; // 光线方向

        return ray(ray_origin, ray_direction);
    }

    [[nodiscard]] ray get_ray_focus(int i, int j) const
    {
        // 构建从散焦圆盘发出并指向像素(i,j)周围随机采样点的相机光线

        auto offset = sample_square(); // 获取方形区域内的随机偏移
        auto pixel_sample = pixel00Loc_ + ((i + offset.x()) * pixelDeltaU_) +
                            ((j + offset.y()) * pixelDeltaV_); // 计算像素采样点

        // NOTE: 3. 光线可以来自聚焦盘
        auto ray_origin =
            (defocus_angle <= 0) ? center_ : defocus_disk_sample(); // 光线起点
        auto ray_direction = pixel_sample - ray_origin;             // 光线方向

        return ray(ray_origin, ray_direction);
    }

    [[nodiscard]] Vec3 sample_square() const
    {
        // 返回[-0.5,-0.5]到[+0.5,+0.5]单位方形区域内的随机点向量
        return {random_double() - 0.5, random_double() - 0.5, 0};
    }

    [[nodiscard]] Vec3 sample_disk(double radius) const
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
        Vec3 unit_direction = unit_vector(r.direction());
        auto a = 0.5 * (unit_direction.y() + 1.0); // 计算垂直方向的混合因子
        // 线性混合：白色（顶部）到蓝色（底部）
        return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
    }
};
