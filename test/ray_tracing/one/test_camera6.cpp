#include "rtweekend.h"

#include "camera.h"
#include "hittable_list.h"
#include "sphere.h"
#include <array>
#include <format>

int main()
{
    hittable_list world;

    world.add(std::make_shared<sphere>(point3(0, 0, -1), 0.5));
    world.add(std::make_shared<sphere>(point3(0, -100.5, -1), 100));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0; // NOLINT
    cam.image_width = 400;         // NOLINT

    cam.samples_per_pixel = 50; // NOLINT
    cam.max_depth = 10;         // NOLINT

    /*
NOTE: gamma 矫正。更平滑的 色彩梯度
*/
    constexpr std::array k_arr = {0.1, 0.3, 0.5, 0.7, 0.9};
    int i = 0;
    for (const auto &reflector : k_arr)
    {
        std::ofstream file(std::format("gamma_no_{}.ppm", i++));
        cam.render_gamma(world, file, reflector, false);
    }
    for (i = 0; const auto &reflector : k_arr)
    {
        std::ofstream file(std::format("gamma_{}.ppm", i++));
        cam.render_gamma(world, file, reflector, true);
    }

    return 0;
}