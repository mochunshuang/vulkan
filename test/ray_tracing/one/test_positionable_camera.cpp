
#include "material.h"

#include "positionable_camera.h"
#include "hittable_list.h"
#include "sphere.h"

#include <fstream>

int main()
{
    hittable_list world;

    auto R = std::cos(pi / 4);

    auto material_left = std::make_shared<lambertian>(color(0, 0, 1));
    auto material_right = std::make_shared<lambertian>(color(1, 0, 0));

    world.add(make_shared<sphere>(point3(-R, 0, -1), R, material_left));
    world.add(make_shared<sphere>(point3(R, 0, -1), R, material_right));

    positionable_camera cam;

    cam.aspect_ratio = 16.0 / 9.0; // NOLINT
    cam.image_width = 400;         // NOLINT
    cam.samples_per_pixel = 100;   // NOLINT
    cam.max_depth = 50;            // NOLINT

    // NOTE:我们将使用90°的视野，通过两个接触球体的简单场景来测试这些变化。
    // cam.vfov = 90; // NOLINT

    /*
    // vfov 越小 = 放大效果（望远镜）
    cam.vfov = 30;   // 窄视角 - 看到的内容少但更大

    // vfov 越大 = 缩小效果（广角镜）
    cam.vfov = 120;  // 宽视角 - 看到的内容多但更小

   vfov ↓ → tan(θ/2) ↓ → viewport_height ↓ → 物体在视口中显得更大
   vfov ↑ → tan(θ/2) ↑ → viewport_height ↑ → 物体在视口中显得更小
   */
    int i = 0;
    for (const auto &vfov : {10, 20, 30, 60, 90, 120})
    {
        cam.vfov = vfov;
        std::ofstream file(std::format("positionable_camera_{}.ppm", i++));
        cam.render(world, file);
    }
    return 0;
}