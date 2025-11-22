
#include "bvh_node.hpp"
#include "hittable_list.hpp"
#include "sphere.hpp"
#include "camera.hpp"

#include <fstream>

// NOLINTBEGIN

void perlin_spheres()
{
    hittable_list world;
    auto pertext = std::make_shared<noise_texture_nosmooth>();
    world.add(
        make_shared<sphere>(point3(0, -1000, 0), 1000, make_shared<lambertian>(pertext)));
    world.add(make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));

    // ==================== 相机设置 ====================
    camera cam;

    // 图像比例：16:9宽屏
    cam.aspect_ratio = 16.0 / 9.0;
    // 图像宽度：400像素（高度自动计算：400/(16/9)=225像素）
    cam.image_width = 400;
    // 每个像素采样次数：100次（用于抗锯齿）
    cam.samples_per_pixel = 100;
    // 光线最大反射深度：50次（控制递归深度）
    cam.max_depth = 50;

    // 视野角度：20度（较小的角度产生"望远镜"效果）
    cam.vfov = 20;

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    // 散焦角度：0度（针孔相机，无景深效果）
    cam.defocus_angle = 0;

    // ==================== 场景构建 ====================
    // 为场景构建BVH加速结构（即使只有一个物体，也保持一致性）
    world = hittable_list(std::make_shared<bvh_node>(world));

    // ==================== 渲染输出 ====================
    // 创建输出文件
    std::ofstream file(std::format("perlin_spheres.ppm"));

    // 开始渲染场景，结果保存到earth.ppm文件
    cam.render(world, file);
}

int main()
{
    // NOTE: 自然球
    perlin_spheres();
}

// NOLINTEND