
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

    cam.vfov = 20;
    cam.lookfrom = point3(-2, 2, 1);
    cam.lookat = point3(0, 0, -1);
    cam.vup = Vec3(0, 1, 0);

    cam.defocus_angle = 10.0;
    cam.focus_dist = 3.4;

    // 渲染到文件
    {
        std::ofstream file(std::format("positionable_focus_no.ppm"));
        cam.render(world, file);
    }

    std::ofstream file(std::format("positionable_focus.ppm"));
    cam.render_focus(world, file);
    return 0;
}