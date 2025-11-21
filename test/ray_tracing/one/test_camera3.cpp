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

    cam.samples_per_pixel = 50; // NOLINT

    std::ofstream file("test_camera3.ppm");
    cam.render_diffuse(world, file);
    return 0;
}