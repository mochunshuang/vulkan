#include "color.h"
#include "ray.h"
#include "vec3.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <ostream>

// NOLINTBEGIN
/*
NOTE: 球体是所有到球心距离等于半径的点的集合
点 P(x,y,z) 在球面上
当且仅当
距离(P, C) = radius

距离(P, C) = √[(x - cx)² + (y - cy)² + (z - cz)²]
那么：
距离(P, C) = radius
↓ 两边平方（消除平方根）
[距离(P, C)]² = radius²
↓ 展开距离公式
(x - cx)² + (y - cy)² + (z - cz)² = radius²

向量形式
设向量 CP = P - C
则 |CP| = radius
|CP|² = radius²
(P - C)·(P - C) = radius²

P = (px, py, pz)    // 点坐标，是向量
C = (cx, cy, cz)    // 球心坐标，是向量
P - C = (px-cx, py-cy, pz-cz)  // 向量减法，结果还是向量

向量长度的定义
向量 v 的长度：|v| = √(v·v)
向量 v 的长度平方：|v|² = v·v

点乘运算：
(P - C)·(P - C) = (px-cx)² + (py-cy)² + (pz-cz)²

点乘（dot product）在物理和几何中有非常重要的意义！
坐标系：
   y
   ↑
   │   B(2,2)
   │   ↗
   │  /
   │ /
   │/  投影到x轴 = 2.0
───●━━━━━━━━━→ x
  O(0,0)  A(3,0)


*/
/*
光线方程：r(t) = origin + t * direction
球体方程：(x - center_x)² + (y - center_y)² + (z - center_z)² = radius²

设 P = r(t) = origin + t * direction
设 C = center
球体方程：(P - C) · (P - C) = radius²
代入光线方程：
(origin + t*direction - C) · (origin + t*direction - C) = radius²

令 oc = origin - C，则：
(oc + t*direction) · (oc + t*direction) = radius²
展开：
(oc·oc) + 2t(oc·direction) + t²(direction·direction) = radius²
整理为标准二次方程形式：
(direction·direction)t² + 2(oc·direction)t + (oc·oc - radius²)=0
*/
// NOTE: 用法向量的方向信息来生成可视化颜色
double hit_sphere(const point3 &center, double radius, const ray &r)
{
    auto oc = center - r.origin();              //  C - origin
    auto a = dot(r.direction(), r.direction()); // direction·direction
    auto b = -2.0 * dot(r.direction(), oc);     // -2 * direction·(C - origin)
    auto c = dot(oc, oc) - radius * radius;     // (C - origin)·(C - origin) - radius²

    auto discriminant = b * b - 4 * a * c;

    //  Δ = b² - 4ac
    //  Δ < 0 确实意味着在实数范围内没有交点，这是由二次方程的数学性质决定的
    if (discriminant < 0)
    {
        return -1.0;
    }
    else
    {
        //  标准二次方程：at² + bt + c = 0
        //  求根公式：t = [-b ± sqrt(b² - 4ac)] / (2a)
        return (-b - std::sqrt(discriminant)) / (2.0 * a);
    }
}

color ray_color(const ray &r)
{
    auto t = hit_sphere(point3(0, 0, -1), 0.5, r);
    if (t > 0.0)
    {
        Vec3 N = unit_vector(r.at(t) - Vec3(0, 0, -1));
        return 0.5 * color(N.x() + 1, N.y() + 1, N.z() + 1);
    }

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
    std::ofstream file("color_by_normal_vectors.ppm");
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