
#include "bvh_node.hpp"
#include "constant_medium.hpp"
#include "hittable_list.hpp"
#include "camera.hpp"

#include <fstream>
#include <thread>

#include "quad.hpp"

// NOLINTBEGIN

void cornell_smoke()
{

    // ==================== 相机设置 ====================
    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    // ==================== 场景构建 ====================
    std::jthread t0{[&] {
        auto red = std::make_shared<lambertian>(color(.65, .05, .05));
        auto white = std::make_shared<lambertian>(color(.73, .73, .73));
        auto green = std::make_shared<lambertian>(color(.12, .45, .15));
        auto light = std::make_shared<diffuse_light>(color(7, 7, 7));

        hittable_list world;
        world.add(make_shared<quad>(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555),
                                    green));
        world.add(
            make_shared<quad>(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
        world.add(make_shared<quad>(point3(113, 554, 127), vec3(330, 0, 0),
                                    vec3(0, 0, 305), light));
        world.add(make_shared<quad>(point3(0, 555, 0), vec3(555, 0, 0), vec3(0, 0, 555),
                                    white));
        world.add(
            make_shared<quad>(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
        world.add(make_shared<quad>(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0),
                                    white));

        std::shared_ptr<hittable> box1 =
            box(point3(0, 0, 0), point3(165, 330, 165), white);
        box1 = make_shared<rotate_y>(box1, 15);
        box1 = make_shared<translate>(box1, vec3(265, 0, 295));

        std::shared_ptr<hittable> box2 =
            box(point3(0, 0, 0), point3(165, 165, 165), white);
        box2 = make_shared<rotate_y>(box2, -18);
        box2 = make_shared<translate>(box2, vec3(130, 0, 65));

        world.add(make_shared<constant_medium>(box1, 0.01, color(0, 0, 0)));
        world.add(make_shared<constant_medium>(box2, 0.01, color(1, 1, 1)));

        std::ofstream file(std::format("cornell_smoke.ppm"));
        cam.render_with_background(hittable_list(std::make_shared<bvh_node>(world)),
                                   file);
    }};
}

int main()
{
    /*
烟雾和雾（深色和浅色粒子）替换这两个块，并使光线更大（并且更暗，这样它就不会吹灭场景）
*/
    cornell_smoke();
}

// NOLINTEND