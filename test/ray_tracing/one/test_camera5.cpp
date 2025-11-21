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

    /*
NOTE: 底部阴影更明显
考虑到我们的两个球体的场景是如此简单，很难区分这两种漫射方法.
    改变后阴影更加明显
    改变后，两个球体都从天空中染成蓝色
*/
    std::ofstream file("test_camera5.ppm");
    cam.render_diffuse2(world, file, true);
    return 0;
}