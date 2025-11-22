
#include "bvh_node.hpp"
#include "hittable_list.hpp"
#include "camera.hpp"

#include <fstream>

#include "quad.hpp"

// NOLINTBEGIN

void test_quad()
{

    // ==================== 相机设置 ====================
    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    cam.vfov = 80;
    cam.lookfrom = point3(0, 0, 9);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    // ==================== 场景构建 ====================
    // Materials
    auto left_red = std::make_shared<lambertian>(color(1.0, 0.2, 0.2));
    auto back_green = std::make_shared<lambertian>(color(0.2, 1.0, 0.2));
    auto right_blue = std::make_shared<lambertian>(color(0.2, 0.2, 1.0));
    auto upper_orange = std::make_shared<lambertian>(color(1.0, 0.5, 0.0));
    auto lower_teal = std::make_shared<lambertian>(color(0.2, 0.8, 0.8));

    // Quads
    hittable_list world;
    world.add(std::make_shared<quad>(point3(-3, -2, 5), vec3(0, 0, -4), vec3(0, 4, 0),
                                     left_red));
    world.add(std::make_shared<quad>(point3(-2, -2, 0), vec3(4, 0, 0), vec3(0, 4, 0),
                                     back_green));
    world.add(std::make_shared<quad>(point3(3, -2, 1), vec3(0, 0, 4), vec3(0, 4, 0),
                                     right_blue));
    world.add(std::make_shared<quad>(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4),
                                     upper_orange));
    world.add(std::make_shared<quad>(point3(-2, -3, 5), vec3(4, 0, 0), vec3(0, 0, -4),
                                     lower_teal));

    std::ofstream file(std::format("test_quad.ppm"));
    cam.render(hittable_list(std::make_shared<bvh_node>(world)), file);
}

int main()
{
    // NOTE: 自然球
    test_quad();
}

// NOLINTEND