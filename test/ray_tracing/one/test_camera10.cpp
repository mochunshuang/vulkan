#include "material.h"
#include "rtweekend.h"

#include "camera.h"
#include "hittable_list.h"
#include "sphere.h"
#include <array>
#include <format>

int main()
{
    hittable_list world;

    auto material_ground = std::make_shared<lambertian>(color(0.8, 0.8, 0.0));
    auto material_center = std::make_shared<lambertian>(color(0.1, 0.2, 0.5));
    // NOTE:我们将模拟一个充满水的世界（折射率约为1.33），并将球体材料更改为空气（折射率为1.00）-一个气泡！
    auto material_left =
        std::make_shared<dielectric_full_internal_reflection>(1.00 / 1.33);
    auto material_right = std::make_shared<metal>(color(0.8, 0.6, 0.2), 1.0);

    world.add(
        std::make_shared<sphere>(point3(0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(std::make_shared<sphere>(point3(0.0, 0.0, -1.2), 0.5, material_center));
    world.add(std::make_shared<sphere>(point3(-1.0, 0.0, -1.0), 0.5, material_left));
    world.add(std::make_shared<sphere>(point3(1.0, 0.0, -1.0), 0.5, material_right));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0; // NOLINTBEGIN
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50; // NOLINTEND

    /*
NOTE: 折射 或 反射. 这里你可以看到或多或少的直射光线会折射，而扫过的光线会反射。
*/
    std::ofstream file(std::format("dielectric_full_internal_reflection.ppm"));
    cam.render_metal(world, file);

    return 0;
}