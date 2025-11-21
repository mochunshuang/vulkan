#include "rtweekend.h"

#include "camera.h"
#include "hittable_list.h"
#include "sphere.h"

int main()
{
    hittable_list world;

    world.add(std::make_shared<sphere>(point3(0, 0, -1), 0.5));
    world.add(std::make_shared<sphere>(point3(0, -100.5, -1), 100));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0; // NOLINT
    cam.image_width = 400;         // NOLINT

    cam.samples_per_pixel = 100; // NOLINT
    cam.max_depth = 50;          // NOLINT

    // NOTE: 避免了错误的阴影。无阴影粉刺的扩散球
    std::ofstream file("test_camera4.ppm");
    cam.render_diffuse2(world, file, false);
    return 0;
}