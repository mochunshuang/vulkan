
#include "bvh_node.hpp"
#include "hittable_list.hpp"
#include "sphere.hpp"
#include "camera.hpp"

#include <fstream>
#include <thread>

// NOLINTBEGIN

void test_noise_texture()
{

    // ==================== 相机设置 ====================
    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    cam.vfov = 20;

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    // 散焦角度：0度（针孔相机，无景深效果）
    cam.defocus_angle = 0;

    // ==================== 场景构建 ====================
    // 为场景构建BVH加速结构（即使只有一个物体，也保持一致性）
    std::jthread t0{[&] {
        constexpr auto scale = 1; // NOTE: 不放大频率
        hittable_list world;
        auto pertext = std::make_shared<noise_texture>(scale);
        world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                      make_shared<lambertian>(pertext)));
        world.add(
            make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));
        std::ofstream file(std::format("noise_texture_0.ppm"));
        cam.render(hittable_list(std::make_shared<bvh_node>(world)), file);
    }};

    std::jthread t1{[&] {
        constexpr auto scale = 4; // NOTE: 放大频率
        hittable_list world;
        auto pertext = std::make_shared<noise_texture>(scale);
        world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                      make_shared<lambertian>(pertext)));
        world.add(
            make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));
        std::ofstream file(std::format("noise_texture_1.ppm"));
        cam.render(hittable_list(std::make_shared<bvh_node>(world)), file);
    }};

    std::jthread t2{[&] {
        constexpr auto scale = 4; // NOTE: 向量随机更合理。向量是由方向的，类似风向
        hittable_list world;
        auto pertext = std::make_shared<noise_texture_with_vec>(scale);
        world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                      make_shared<lambertian>(pertext)));
        world.add(
            make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));
        std::ofstream file(std::format("noise_texture_2.ppm"));
        cam.render(hittable_list(std::make_shared<bvh_node>(world)), file);
    }};

    std::jthread t3{[&] {
        constexpr auto scale = 4; // NOTE: 叠加多个不同频率的噪声来产生更复杂的自然图案
        hittable_list world;
        auto pertext = std::make_shared<noise_texture_with_vec_and_turb>(scale);
        world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                      make_shared<lambertian>(pertext)));
        world.add(
            make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));
        std::ofstream file(std::format("noise_texture_3.ppm"));
        cam.render(hittable_list(std::make_shared<bvh_node>(world)), file);
    }};

    std::jthread t4{[&] {
        constexpr auto scale = 4; // NOTE: 相位扰动，形成大理石花纹
        hittable_list world;
        auto pertext = std::make_shared<noise_texture_with_vec_and_turb_phase>(scale);
        world.add(make_shared<sphere>(point3(0, -1000, 0), 1000,
                                      make_shared<lambertian>(pertext)));
        world.add(
            make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));
        std::ofstream file(std::format("noise_texture_4.ppm"));
        cam.render(hittable_list(std::make_shared<bvh_node>(world)), file);
    }};

    t0.join();
    t1.join();
    t2.join();
    t3.join();
    t4.join();
}

int main()
{
    // NOTE: 自然球
    test_noise_texture();
}

// NOLINTEND