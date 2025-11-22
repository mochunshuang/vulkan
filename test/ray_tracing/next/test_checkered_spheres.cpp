
#include "bvh_node.hpp"
#include "hittable_list.hpp"
#include "sphere.hpp"
#include "camera.hpp"

#include <fstream>

// NOLINTBEGIN

void checkered_spheres()
{
    hittable_list world;

    auto checker =
        std::make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9, .9, .9));

    world.add(
        make_shared<sphere>(point3(0, -10, 0), 10, make_shared<lambertian>(checker)));
    world.add(
        make_shared<sphere>(point3(0, 10, 0), 10, make_shared<lambertian>(checker)));

    world = hittable_list(std::make_shared<bvh_node>(world));
    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;
    std::ofstream file(std::format("checkered_spheres.ppm"));
    cam.render(world, file);
}

int main()
{
    // NOTE: 方格球体
    checkered_spheres();
}

// NOLINTEND