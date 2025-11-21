#include "color.h"
#include "ray.h"
#include "vec3.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <ostream>

// NOLINTBEGIN
// NOTE: 求解二次方程来测试光线是否与球体相交
bool hit_sphere(const point3 &center, double radius, const ray &r)
{
    // 球体与光线求交的数学公式：
    // 球体方程: (P - C)·(P - C) = R²
    // 光线方程: P(t) = A + tB
    // 联立得: (A + tB - C)·(A + tB - C) = R²
    // 展开: t²(B·B) + 2tB·(A-C) + (A-C)·(A-C) - R² = 0

    auto oc = center - r.origin();              // (A - C)：从光线起点到球心的向量
    auto a = dot(r.direction(), r.direction()); // (B·B)：光线方向向量的点积
    auto b = -2.0 * dot(r.direction(), oc);     // -2B·(A-C)：一次项系数
    auto c = dot(oc, oc) - radius * radius;     // (A-C)·(A-C) - R²：常数项
    auto discriminant = b * b - 4 * a * c;      // 判别式：b² - 4ac

    return (discriminant >= 0); // 判别式≥0表示有交点
}

color ray_color(const ray &r)
{
    // constexpr auto radius = 1; // NOTE: 整个屏幕是红色
    // constexpr auto radius = 0.5; // NOTE: 直接占用高度一半的球
    constexpr auto radius = 0.25; // 球直径小一半

    // constexpr auto center = point3(0, 0, -1);
    // constexpr auto center = point3(0, 0, -2); // 远离相机 → 投影变小
    constexpr auto center = point3(0, 0, -0.5); // 变大
    if (hit_sphere(center, radius, r))
        return color(1, 0, 0); // NOTE: 和球体相交的像素，呈现红色。这就是光线追踪

    constexpr auto start_v = color(1.0, 1.0, 1.0);
    constexpr auto end_v = color(0.5, 0.7, 1.0);

    // 第1步：将光线方向向量单位化
    Vec3 unit_direction = unit_vector(r.direction());
    // 作用：确保向量长度为1，这样y分量就在[-1, 1]范围内

    // 第2步：将y分量从[-1, 1]映射到[0, 1]
    auto a = 0.5 * (unit_direction.y() + 1.0);

    assert(a > 0);

    // 第3步：线性插值混合颜色
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
    constexpr auto focal_length = 1.0; // 焦距：相机到视口平面的距离
    // constexpr auto viewport_height = 2.0; // 视口高度
    // constexpr auto viewport_height = 1.0; // 缩小视口 → 球体相对变大
    constexpr auto viewport_height = 3.0; // 球体变小

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
    std::ofstream file("first_raytraced_image.ppm");
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