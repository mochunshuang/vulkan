
#include "bvh_node.hpp"
#include "constant_medium.hpp"
#include "hittable_list.hpp"
#include "camera.hpp"

#include <fstream>
#include <thread>

#include "quad.hpp"
#include "sphere.hpp"

// NOLINTBEGIN

void final_scene(int image_width, int samples_per_pixel, int max_depth, int i)
{

    // ==================== 相机设置 ====================
    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(478, 278, -600);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    // ==================== 场景构建 ====================
    hittable_list boxes1;
    auto ground = std::make_shared<lambertian>(color(0.48, 0.83, 0.53));

    int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++)
    {
        for (int j = 0; j < boxes_per_side; j++)
        {
            auto w = 100.0;
            auto x0 = -1000.0 + i * w;
            auto z0 = -1000.0 + j * w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1, 101);
            auto z1 = z0 + w;

            boxes1.add(box(point3(x0, y0, z0), point3(x1, y1, z1), ground));
        }
    }

    hittable_list world;

    world.add(std::make_shared<bvh_node>(boxes1));

    auto light = std::make_shared<diffuse_light>(color(7, 7, 7));
    world.add(make_shared<quad>(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265),
                                light));

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30, 0, 0);
    auto sphere_material = std::make_shared<lambertian>(color(0.7, 0.3, 0.1));
    world.add(make_shared<sphere>(center1, center2, 50, sphere_material));

    world.add(
        make_shared<sphere>(point3(260, 150, 45), 50, std::make_shared<dielectric>(1.5)));
    world.add(make_shared<sphere>(point3(0, 150, 145), 50,
                                  std::make_shared<metal>(color(0.8, 0.8, 0.9), 1.0)));

    auto boundary =
        make_shared<sphere>(point3(360, 150, 145), 70, std::make_shared<dielectric>(1.5));
    world.add(boundary);
    world.add(make_shared<constant_medium>(boundary, 0.2, color(0.2, 0.4, 0.9)));
    boundary =
        make_shared<sphere>(point3(0, 0, 0), 5000, std::make_shared<dielectric>(1.5));
    world.add(make_shared<constant_medium>(boundary, .0001, color(1, 1, 1)));

    auto emat = make_shared<lambertian>(std::make_shared<image_texture>("earthmap.jpg"));
    world.add(make_shared<sphere>(point3(400, 200, 400), 100, emat));
    auto pertext = std::make_shared<noise_texture_with_vec_and_turb_phase>(0.2);
    world.add(
        make_shared<sphere>(point3(220, 280, 300), 80, make_shared<lambertian>(pertext)));

    hittable_list boxes2;
    auto white = std::make_shared<lambertian>(color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++)
    {
        boxes2.add(make_shared<sphere>(point3::random(0, 165), 10, white));
    }

    world.add(make_shared<translate>(
        make_shared<rotate_y>(std::make_shared<bvh_node>(boxes2), 15),
        vec3(-100, 270, 395)));

    std::ofstream file(std::format("final_scene_{}.ppm", i));
    cam.render_with_background(hittable_list(std::make_shared<bvh_node>(world)), file);
}

int main()
{
    /*
让我们把它们放在一起，用一个大薄雾覆盖一切，和一个蓝色的地下反射球
（我们没有明确实现这一点，但是电介质内部的体积就是地下材料）。
渲染器中留下的最大限制是没有阴影光线，但这就是为什么我们免费获得焦散线和地下。这是一个双刃剑的设计决定.
另请注意，我们将参数化这个最终场景以支持较低质量的渲染以进行快速测试。
*/
    std::jthread t0([] { final_scene(800, 10000, 40, 0); });
    std::jthread t1([] { final_scene(400, 250, 4, 1); });

    t0.join();
    t1.join();
    return 0;
}

// NOLINTEND