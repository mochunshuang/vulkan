
#include "bvh_node.hpp"
#include "hittable_list.hpp"
#include "camera.hpp"

#include <fstream>
#include <thread>

#include "quad.hpp"

// NOLINTBEGIN

void cornell_box()
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
    // Quads

    std::jthread t0{[&] {
        hittable_list world;

        auto red = std::make_shared<lambertian>(color(.65, .05, .05));
        auto white = std::make_shared<lambertian>(color(.73, .73, .73));
        auto green = std::make_shared<lambertian>(color(.12, .45, .15));
        auto light = std::make_shared<diffuse_light>(color(15, 15, 15));

        world.add(make_shared<quad>(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555),
                                    green));
        world.add(
            make_shared<quad>(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
        world.add(make_shared<quad>(point3(343, 554, 332), vec3(-130, 0, 0),
                                    vec3(0, 0, -105), light));
        world.add(
            make_shared<quad>(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
        world.add(make_shared<quad>(point3(555, 555, 555), vec3(-555, 0, 0),
                                    vec3(0, 0, -555), white));
        world.add(make_shared<quad>(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0),
                                    white));

        std::ofstream file(std::format("cornell_box_0.ppm"));
        cam.render_with_background(hittable_list(std::make_shared<bvh_node>(world)),
                                   file);
    }};

    //
    std::jthread t1{[&] {
        hittable_list world;

        auto red = std::make_shared<lambertian>(color(.65, .05, .05));
        auto white = std::make_shared<lambertian>(color(.73, .73, .73));
        auto green = std::make_shared<lambertian>(color(.12, .45, .15));
        auto light = std::make_shared<diffuse_light>(color(15, 15, 15));

        world.add(make_shared<quad>(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555),
                                    green));
        world.add(
            make_shared<quad>(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
        world.add(make_shared<quad>(point3(343, 554, 332), vec3(-130, 0, 0),
                                    vec3(0, 0, -105), light));
        world.add(
            make_shared<quad>(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
        world.add(make_shared<quad>(point3(555, 555, 555), vec3(-555, 0, 0),
                                    vec3(0, 0, -555), white));
        world.add(make_shared<quad>(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0),
                                    white));

        // NOTE: 添加两个箱子
        world.add(box(point3(130, 0, 65), point3(295, 165, 230), white));
        world.add(box(point3(265, 0, 295), point3(430, 330, 460), white));

        std::ofstream file(std::format("cornell_box_1.ppm"));
        cam.render_with_background(hittable_list(std::make_shared<bvh_node>(world)),
                                   file);
    }};

    std::jthread t2{[&] {
        hittable_list world;

        auto red = std::make_shared<lambertian>(color(.65, .05, .05));
        auto white = std::make_shared<lambertian>(color(.73, .73, .73));
        auto green = std::make_shared<lambertian>(color(.12, .45, .15));
        auto light = std::make_shared<diffuse_light>(color(15, 15, 15));

        world.add(make_shared<quad>(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555),
                                    green));
        world.add(
            make_shared<quad>(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
        world.add(make_shared<quad>(point3(343, 554, 332), vec3(-130, 0, 0),
                                    vec3(0, 0, -105), light));
        world.add(
            make_shared<quad>(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
        world.add(make_shared<quad>(point3(555, 555, 555), vec3(-555, 0, 0),
                                    vec3(0, 0, -555), white));
        world.add(make_shared<quad>(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0),
                                    white));

        // NOTE: 添加两个箱子，不过是旋转后的
        std::shared_ptr<hittable> box1 =
            box(point3(0, 0, 0), point3(165, 330, 165), white);
        box1 = make_shared<rotate_y>(box1, 15);
        box1 = make_shared<translate>(box1, vec3(265, 0, 295));
        world.add(box1);

        std::shared_ptr<hittable> box2 =
            box(point3(0, 0, 0), point3(165, 165, 165), white);
        box2 = make_shared<rotate_y>(box2, -18);
        box2 = make_shared<translate>(box2, vec3(130, 0, 65));
        world.add(box2);

        std::ofstream file(std::format("cornell_box_2.ppm"));
        cam.render_with_background(hittable_list(std::make_shared<bvh_node>(world)),
                                   file);
    }};
}

int main()
{
    /*
“康奈尔盒子”于1984年推出，用于模拟漫射表面之间的光相互作用。让我们制作5个墙壁和盒子的光：
*/
    cornell_box();
}

// NOLINTEND