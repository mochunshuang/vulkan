#include "color.h"
#include "ray.h"
#include "vec3.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <ostream>

// NOLINTBEGIN
/*
a = direction·direction
b = -2 * (direction·oc)
c = oc·oc - radius²

令 h = direction·oc，则 b = -2h
Δ = b² - 4ac = (-2h)² - 4a*c = 4h² - 4ac = 4(h² - ac)
优化后判别式：Δ = h² - ac
省去了乘以4的操作，因为开方时 √(4x) = 2√x 会抵消

求根公式简化
t = (-b - √Δ) / (2a) = (2h - 2√(h²-ac)) / (2a) = (h - √(h²-ac)) / a
t = (h - √discriminant) / a
*/
// NOTE: 用法向量的方向信息来生成可视化颜色
double hit_sphere(const point3 &center, double radius, const ray &r)
{
    auto oc = center - r.origin();
    auto a = r.direction().length_squared();
    auto h = dot(r.direction(), oc);
    auto c = oc.length_squared() - radius * radius;

    auto discriminant = h * h - a * c;
    if (discriminant < 0)
    {
        return -1.0;
    }
    else
    {
        return (h - std::sqrt(discriminant)) / a;
    }
}

// NOTE: 生成目的的图片就行
color ray_color(const ray &r)
{
    // NOTE: 多边形颜色
    auto t = hit_sphere(point3(0, 0, -1), 0.5, r);
    if (t > 0.0)
    {
        Vec3 N = unit_vector(r.at(t) - Vec3(0, 0, -1));
        return 0.5 * color(N.x() + 1, N.y() + 1, N.z() + 1);
    }

    // NOTE: 背景色
    constexpr auto start_v = color(1.0, 1.0, 1.0);
    constexpr auto end_v = color(0.5, 0.7, 1.0);
    Vec3 unit_direction = unit_vector(r.direction());
    auto a = 0.5 * (unit_direction.y() + 1.0);
    return (1.0 - a) * start_v + a * end_v;
}

int main()
{

    // 图像设置: 宽高
    constexpr auto aspect_ratio = 16.0 / 9.0; // 宽高比 16:9
    constexpr int image_width = 400;

    // 计算图像高度，并确保至少为1像素
    constexpr int ratio = int(image_width / aspect_ratio); // 400 / (16/9) = 225
    constexpr auto image_height = (ratio < 1) ? 1 : ratio;

    // 相机设置
    constexpr auto focal_length = 1.0;    // 焦距：相机到视口平面的距离
    constexpr auto viewport_height = 2.0; // 视口高度

    constexpr auto viewport_width =
        viewport_height * (double(image_width) / image_height);
    constexpr auto camera_center = point3(0, 0, 0); // 相机位置：世界坐标系原点

    // 计算视口水平方向和垂直方向的边界向量
    constexpr auto viewport_u = Vec3(viewport_width, 0, 0); // 水平向右的向量
    constexpr auto viewport_v = Vec3(0, -viewport_height, 0);

    // 计算像素之间的水平和垂直增量向量
    constexpr auto pixel_delta_u = viewport_u / image_width;  // 每个像素水平移动的距离
    constexpr auto pixel_delta_v = viewport_v / image_height; // 每个像素垂直移动的距离

    // 计算左上角第一个像素的位置
    // 视口左上角 = 相机位置 - 焦距向量 - 水平半宽 - 垂直半高
    constexpr auto viewport_upper_left =
        camera_center - Vec3(0, 0, focal_length) - viewport_u / 2 - viewport_v / 2;
    // 第一个像素(0,0)的中心位置 = 视口左上角 + 半个像素偏移（移动到第一个像素中心）
    constexpr auto pixel00_loc =
        viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // 渲染
    std::ofstream file("color_by_normal_vectors2.ppm");
    file << "P3\n" << image_width << " " << image_height << "\n255\n";

    // NOTE: 生成图像的像素
    for (int j = 0; j < image_height; j++)
    {
        std::cout << "\rScanlines remaining: " << (image_height - j) << '\n';
        for (int i = 0; i < image_width; i++)
        {
            // 计算当前像素中心的世界坐标
            // 从第一个像素中心开始，向右移动i个像素，向下移动j个像素
            auto pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);

            // 计算光线方向：从相机位置指向当前像素中心
            auto ray_direction = pixel_center - camera_center;

            // NOTE: 从相机发出光
            ray r(camera_center, ray_direction); // 创建光线
            color pixel_color = ray_color(r);    // 计算该光线看到的颜色

            write_color(file, pixel_color); // 将颜色写入PPM文件
        }
    }

    std::cout << "\rDone." << std::endl; // 渲染完成
}
// NOLINTEND