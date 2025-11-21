#include "color.h"
#include "ray.h"
#include "vec3.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <ostream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// NOLINTBEGIN
color ray_color(const ray &r)
{
    constexpr auto start_v = color(1.0, 1.0, 1.0);
    constexpr auto end_v = color(0.5, 0.7, 1.0);
    // return color(0, 0, 0); //NOTE: 黑色
    // return start_v; // NOTE: 白色
    // return end_v; // NOTE: 蓝色

    // NOTE: 4. 得到向量的单位向量
    Vec3 unit_direction = unit_vector(r.direction());

    // NOTE: 线性变换，相对关系不变。相对
    //  第2步：将y分量从[-1, 1]映射到[0, 1]
    //  [-1, 1] -> [0,2] -> [0,1]
    //  y_new = (y_old + 1) / 2
    auto a = (unit_direction.y() + 1.0) / 2;

    glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    assert(a > 0);

    // // 第3步：线性插值混合颜色
    return (1.0 - a) * start_v + a * end_v;
}

int main()
{
    // ==================== 坐标系定义 ====================
    // 世界坐标系（右手系）：
    //
    //        Y ↑
    //          |
    //          |
    //          |
    //          +--------→ X
    //         /
    //        /
    //       Z (指向屏幕外)
    //
    // 屏幕像素坐标系：
    //
    //    (0,0) ·--------> +i (X方向，向右)
    //          |
    //          |
    //          |
    //          ↓ +j (Y方向，向下)
    //
    // 注意：世界坐标Y向上，屏幕坐标Y向下！
    // ==================================================
    // NOTE: 确定 Z轴 和 右手坐标系。就能确定Z,X，Y各自的正方向
    //  ==================== 完整的坐标系定义 ====================
    //  使用右手坐标系：
    //    X轴: 水平向右 (正方向)
    //    Y轴: 垂直向上 (正方向)
    //    Z轴: 从屏幕指向外面 (正方向) - 右手定则
    //
    //  原点定义：
    //    世界坐标系原点 (0,0,0) = 相机位置（眼睛位置）
    //
    //  相机朝向：看向 -Z 方向（指向屏幕内部）
    //  视口平面：位于 Z = -1 的平面上
    //
    //  视口中心：位于 (0, 0, -1)
    //  视口范围：X从 -1.777 到 +1.777，Y从 -1.0 到 +1.0
    //
    //  屏幕像素坐标：
    //    (0,0)    = 左上角第一个像素
    //    (399,0)  = 右上角像素
    //    (0,224)  = 左下角像素
    //    (399,224)= 右下角像素
    //  ========================================================

    /*
    NOTE: 核心价值：没有视口，相机就不知道往哪里看；有了视口，就能精确控制渲染范围！
    ══════════ 现实世界比喻 ══════════
    您站在窗前（相机）              窗外是世界（3D场景）
    ┌─────────────────┐           ┌─────────────────┐
    │                 │           │    🏠   🌳     │
    │      视口        │ ← 窗框    │        🚗      │
    │    (Viewport)   │           │                 │
    │                 │           │     ☁️  ☁️      │
    └─────────────────┘           └─────────────────┘
        您的窗户                       窗外风景

    ══════════ 光线追踪系统 ══════════
    相机(眼睛)位置                  视口(成像平面)               3D场景
    👁️                        ┌─────────────────┐        🟦🟦🟦
   (0,0,0)                    │●  │   │   │   │●│       🟦🌍🟦
      |                       ├───┼───┼───┼───┤        🟦🟦🟦
      | 光线 → → → → → → → → →│ → │ → │ → │ → │→ → → → 🟥🟥🟥
      |                       ├───┼───┼───┼───┤        🟥🏠🟥
      | 光线 → → → → → → → → →│ → │ → │ → │ → │→ → → → 🟥🟥🟥
      |                       ├───┼───┼───┼───┤        🟩🟩🟩
      | 光线 → → → → → → → → →│ → │ → │ → │ → │→ → → → 🟩🌳🟩
      ↓                       └─────────────────┘        🟩🟩🟩
   Z=0平面                     Z=-1平面(视口)            Z<-1场景


    视口外 → 不可见               视口内 → 可见
                             ┌─────────────────┐
    🌄                      │     🏠   🌳     │ ← 只渲染这里的物体
                             │          🚗      │
    🏔️                      └─────────────────┘
*/

    // 图像设置: 宽高。//NOTE:目的投影
    constexpr auto aspect_ratio = 16.0 / 9.0; // 宽高比 16:9
    constexpr int image_width = 400;

    // 计算图像高度，并确保至少为1像素
    constexpr int ratio = int(image_width / aspect_ratio); // 400 / (16/9) = 225
    constexpr auto image_height = (ratio < 1) ? 1 : ratio;

    /*
    NOTE: 2.必须保持耦合，因为：
        物理正确性：保证光线追踪的几何关系正确
        无变形渲染：避免图像拉伸或压缩
        像素对齐：确保每个像素均匀覆盖视口区域
    */
    // 相机设置
    constexpr auto focal_length = 1.0;    // 焦距：相机到视口平面的距离
    constexpr auto viewport_height = 2.0; // 视口高度
    constexpr auto viewport_width =
        viewport_height * (double(image_width) / image_height);
    constexpr auto camera_center = point3(0, 0, 0); // 相机位置：世界坐标系原点

    // 计算视口水平方向和垂直方向的边界向量
    constexpr auto viewport_u = Vec3(viewport_width, 0, 0); // 水平向右的向量
    constexpr auto viewport_v =
        Vec3(0, -viewport_height, 0); // 垂直向下的向量（负号表示Y轴向下）

    // 计算像素之间的水平和垂直增量向量
    constexpr auto pixel_delta_u = viewport_u / image_width;  // 每个像素水平移动的距离
    constexpr auto pixel_delta_v = viewport_v / image_height; // 每个像素垂直移动的距离

    // 计算左上角第一个像素的位置
    // 视口左上角 = 相机位置 - 焦距向量 - 水平半宽 - 垂直半高
    constexpr auto viewport_upper_left =
        camera_center - Vec3(0, 0, focal_length) - viewport_u / 2 - viewport_v / 2;
    // NOTE:第一个像素(0,0)的中心位置 = 视口左上角 + 半个像素偏移（移动到第一个像素中心）
    constexpr auto pixel00_loc =
        viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    // NOTE:1. 像素和像素的坐标，还是有区别的。像素的矩形的中心，才是像素中心

    // 渲染
    std::ofstream file("test_ray.ppm");
    file << "P3\n" << image_width << " " << image_height << "\n255\n";

    // NOTE: 生成图像的像素
    for (int j = 0; j < image_height; j++)
    {
        std::cout << "\rScanlines remaining: " << (image_height - j) << '\n';
        for (int i = 0; i < image_width; i++)
        {
            // NOTE: 3. 推导出其他像素中心
            auto pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);

            // NOTE: 4. 光线方向
            auto ray_direction = pixel_center - camera_center;

            // NOTE: 从相机发出光
            ray r(camera_center, ray_direction); // 创建光线

            color pixel_color = ray_color(r);
            write_color(file, pixel_color);
        }
    }

    std::cout << "\rDone." << std::endl; // 渲染完成
}
// NOLINTEND