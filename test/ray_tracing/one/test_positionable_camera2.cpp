
#include "material.h"

#include "positionable_camera.h"
#include "hittable_list.h"
#include "sphere.h"

#include <fstream>

int main()
{
    hittable_list world;

    auto material_ground = std::make_shared<lambertian>(color(0.8, 0.8, 0.0));
    auto material_center = std::make_shared<lambertian>(color(0.1, 0.2, 0.5));
    auto material_left = std::make_shared<dielectric>(1.50);
    auto material_bubble = std::make_shared<dielectric>(1.00 / 1.50);
    auto material_right = std::make_shared<metal>(color(0.8, 0.6, 0.2), 1.0);

    world.add(make_shared<sphere>(point3(0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(make_shared<sphere>(point3(0.0, 0.0, -1.2), 0.5, material_center));
    world.add(make_shared<sphere>(point3(-1.0, 0.0, -1.0), 0.5, material_left));
    world.add(make_shared<sphere>(point3(-1.0, 0.0, -1.0), 0.4, material_bubble));
    world.add(make_shared<sphere>(point3(1.0, 0.0, -1.0), 0.5, material_right));

    positionable_camera cam;

    cam.aspect_ratio = 16.0 / 9.0; // NOLINT
    cam.image_width = 400;         // NOLINT
    cam.samples_per_pixel = 100;   // NOLINT
    cam.max_depth = 50;            // NOLINT

    // 2×2×2 参数组合测试
    std::vector<double> vfov_values = {60, 120}; // 2个FOV值：窄角和广角
    std::vector<point3> lookfrom_positions = {
        point3(-2, 2, 1), // 视角1：左上方
        point3(2, 1, 0)   // 视角2：右前方
    };
    std::vector<point3> lookat_targets = {
        point3(0, 0, -1), // 目标1：场景后部
        point3(0, 0, 1)   // 目标2：场景前部
    };
    // vup 保持不变
    Vec3 fixed_vup = Vec3(0, 1, 0);

    int i = 0;
    for (auto vfov : vfov_values)
    {
        for (auto lookfrom : lookfrom_positions)
        {
            for (auto lookat : lookat_targets)
            {
                cam.vfov = vfov;
                cam.lookfrom = lookfrom;
                cam.lookat = lookat;
                cam.vup = fixed_vup;

                // 渲染到文件
                std::ofstream file(std::format("positionable_look_{}.ppm", i++));
                cam.render(world, file);
            }
        }
    }
    return 0;
}