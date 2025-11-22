
#include "bvh_node.hpp"
#include "hittable_list.hpp"
#include "sphere.hpp"
#include "camera.hpp"

#include <fstream>

// NOLINTBEGIN

void earth()
{
    // NOTE: 会自动找寻上级 images文件夹
    auto earth_texture = std::make_shared<image_texture>("earthmap.jpg");
    // 创建朗伯材质（漫反射材质），使用地球纹理
    auto earth_surface = std::make_shared<lambertian>(earth_texture);

    // 创建球体对象：
    // - 中心点：世界坐标系原点(0,0,0)
    // - 半径：2个单位
    // - 表面材质：地球纹理材质
    auto globe = std::make_shared<sphere>(point3(0, 0, 0), 2, earth_surface);

    // ==================== 相机设置 ====================
    camera cam;

    // 图像比例：16:9宽屏
    cam.aspect_ratio = 16.0 / 9.0;

    // 图像宽度：400像素（高度自动计算：400/(16/9)=225像素）
    cam.image_width = 400;

    // 每个像素采样次数：100次（用于抗锯齿）
    cam.samples_per_pixel = 100;

    // 光线最大反射深度：50次（控制递归深度）
    cam.max_depth = 50;

    // 视野角度：20度（较小的角度产生"望远镜"效果）
    cam.vfov = 20;

    // 相机位置：在Z轴上，距离原点12个单位
    cam.lookfrom = point3(0, 0, 12);

    // 相机看向的点：世界坐标系原点（地球中心）
    cam.lookat = point3(0, 0, 0);

    // 相机上方向向量：Y轴正方向
    cam.vup = vec3(0, 1, 0);

    // 散焦角度：0度（针孔相机，无景深效果）
    cam.defocus_angle = 0;

    // ==================== 场景构建 ====================
    hittable_list world;

    // 将地球球体添加到场景中
    world.add(globe);

    // 为场景构建BVH加速结构（即使只有一个物体，也保持一致性）
    world = hittable_list(std::make_shared<bvh_node>(world));

    // ==================== 渲染输出 ====================
    // 创建输出文件
    std::ofstream file(std::format("earth.ppm"));

    // 开始渲染场景，结果保存到earth.ppm文件
    cam.render(world, file);
}

int main()
{
    // NOTE: 世界地图
    earth();
}

// NOLINTEND