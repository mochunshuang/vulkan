
#include "material.h"

#include "positionable_camera.h"
#include "hittable_list.h"
#include "sphere.h"

#include <fstream>
#include <thread>

// NOLINTBEGIN
int main()
{
    hittable_list world;

    auto ground_material = std::make_shared<lambertian>(color(0.5, 0.5, 0.5));
    world.add(std::make_shared<sphere>(point3(0, -1000, 0), 1000, ground_material));

    for (int a = -11; a < 11; a++)
    {
        for (int b = -11; b < 11; b++)
        {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9)
            {
                std::shared_ptr<material> sphere_material;

                if (choose_mat < 0.8)
                {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    sphere_material = std::make_shared<lambertian>(albedo);
                    world.add(std::make_shared<sphere>(center, 0.2, sphere_material));
                }
                else if (choose_mat < 0.95)
                {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = std::make_shared<metal>(albedo, fuzz);
                    world.add(std::make_shared<sphere>(center, 0.2, sphere_material));
                }
                else
                {
                    // glass
                    sphere_material = std::make_shared<dielectric>(1.5);
                    world.add(std::make_shared<sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = std::make_shared<dielectric>(1.5);
    world.add(std::make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

    auto material2 = std::make_shared<lambertian>(color(0.4, 0.2, 0.1));
    world.add(std::make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

    auto material3 = std::make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    world.add(std::make_shared<sphere>(point3(4, 1, 0), 1.0, material3));
    positionable_camera cam;

    // NOTE: 慢的要死。几首歌的时间
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 800;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = Vec3(0, 1, 0);

    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    /*
你可能会注意到一件有趣的事情，玻璃球并没有真正的阴影，这让它们看起来像是漂浮的。这不是一个bug——你在现实生活中很少看到玻璃球，它们看起来也有点奇怪，确实看起来像是在阴天漂浮。玻璃球下的大球体上的一个点仍然有很多光线照射它，因为天空是重新排序的，而不是被阻挡的
*/
    // 渲染到文件
    std::jthread t0([&] {
        std::ofstream file(std::format("final_scene_focus_no.ppm"));
        cam.render(world, file);
    });

    std::jthread t1([&] {
        std::ofstream file(std::format("final_scene_focus.ppm"));
        cam.render_focus(world, file);
    });

    t0.join();
    t1.join();
    return 0;
}
// NOLINTEND