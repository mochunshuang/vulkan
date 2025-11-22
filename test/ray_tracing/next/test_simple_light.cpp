
#include "bvh_node.hpp"
#include "hittable_list.hpp"
#include "camera.hpp"

#include <fstream>
#include <thread>

#include "quad.hpp"
#include "sphere.hpp"

// NOLINTBEGIN

void test_simple_light()
{

    // ==================== 相机设置 ====================
    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0, 0, 0);

    cam.vfov = 20;
    cam.lookfrom = point3(26, 3, 6);
    cam.lookat = point3(0, 2, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    // ==================== 场景构建 ====================
    // Quads

    std::jthread t0{[&] {
        hittable_list world;
        auto pertext = std::make_shared<noise_texture_with_vec_and_turb_phase>(4);
        world.add(std::make_shared<sphere>(point3(0, -1000, 0), 1000,
                                           make_shared<lambertian>(pertext)));
        world.add(
            make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));

        auto difflight = std::make_shared<diffuse_light>(color(4, 4, 4));
        world.add(
            make_shared<quad>(point3(3, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0), difflight));

        std::ofstream file(std::format("simple_light_0.ppm"));
        cam.render_with_background(hittable_list(std::make_shared<bvh_node>(world)),
                                   file);
    }};

    std::jthread t1{[&] {
        hittable_list world;
        auto pertext = std::make_shared<noise_texture_with_vec_and_turb_phase>(4);
        world.add(std::make_shared<sphere>(point3(0, -1000, 0), 1000,
                                           make_shared<lambertian>(pertext)));
        world.add(
            make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));

        auto difflight = std::make_shared<diffuse_light>(color(4, 4, 4));

        // NOTE: 添加新光源
        world.add(make_shared<sphere>(point3(0, 7, 0), 2, difflight));
        world.add(
            make_shared<quad>(point3(3, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0), difflight));

        std::ofstream file(std::format("simple_light_1.ppm"));
        cam.render_with_background(hittable_list(std::make_shared<bvh_node>(world)),
                                   file);
    }};
}

int main()
{
    // NOTE: 将物体变成灯
    test_simple_light();
}

// NOLINTEND