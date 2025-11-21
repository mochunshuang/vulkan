#pragma once

#include "color.h"
#include "hittable.h"

#include "material.h"

/*
NOTE: 责任如下
1. 构建并向世界发送光线。
2. 使用这些光线的结果来构建渲染图像
*/
class camera
{
  public:
    double aspect_ratio = 1.0; // Ratio of image width over height
    int image_width = 100;     // Rendered image width in pixel count

    // NOTE: 用于反走样
    int samples_per_pixel = 10; // Count of random samples for each pixel

    // NOTE: 漫反射总跳最大次数
    int max_depth = 10; // Maximum number of ray bounces into scene

    void render(const hittable &world, std::ostream &out_stream)
    {
        initialize();

        out_stream << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++)
        {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' '
                      << std::flush;
            for (int i = 0; i < image_width; i++)
            {
                auto pixel_center =
                    pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
                auto ray_direction = pixel_center - center;
                ray r(center, ray_direction);

                color pixel_color = ray_color(r, world);
                write_color(out_stream, pixel_color);
            }
        }

        std::cout << "\rDone.                 \n";
    }
    void render_antialiasing(const hittable &world, std::ostream &out)
    {
        initialize();

        out << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++)
        {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' '
                      << std::flush;
            for (int i = 0; i < image_width; i++)
            {
                color pixel_color(0, 0, 0);
                for (int sample = 0; sample < samples_per_pixel; sample++)
                {
                    /*
                    ┌───┬───┬───┐
                    │ · │·  │   │
                    │·  │  ·│ · │  ← 完全随机的采样点
                    │  ·│·  │   │
                    └───┴───┴───┘
                    */
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, world);
                }
                write_color(out, pixel_samples_scale * pixel_color);
            }
        }

        std::cout << "\rDone.                 \n";
    }
    void render_diffuse(const hittable &world, std::ostream &out)
    {
        initialize();

        out << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++)
        {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' '
                      << std::flush;
            for (int i = 0; i < image_width; i++)
            {
                color pixel_color(0, 0, 0);
                for (int sample = 0; sample < samples_per_pixel; sample++)
                {
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, world, true);
                }
                write_color(out, pixel_samples_scale * pixel_color);
            }
        }

        std::cout << "\rDone.                 \n";
    }

    void render_diffuse2(const hittable &world, std::ostream &out, bool lambertian = true)
    {
        initialize();

        out << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++)
        {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' '
                      << std::flush;
            for (int i = 0; i < image_width; i++)
            {
                color pixel_color(0, 0, 0);
                for (int sample = 0; sample < samples_per_pixel; sample++)
                {
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, max_depth, world, lambertian);
                }
                write_color(out, pixel_samples_scale * pixel_color);
            }
        }

        std::cout << "\rDone.                 \n";
    }

    void render_gamma(const hittable &world, std::ostream &out, double reflector,
                      bool gamma = true)
    {
        initialize();

        constexpr auto k_ray_color = [](this auto self, const ray &r, int depth,
                                        const hittable &world, double reflector) {
            // If we've exceeded the ray bounce limit, no more light is gathered.
            if (depth <= 0)
                return color(0, 0, 0);

            hit_record rec;

            if (world.hit(r, interval(0.001, infinity), rec))
            {
                Vec3 direction = rec.normal + random_unit_vector();
                return reflector *
                       self(ray(rec.p, direction), depth - 1, world, reflector);
            }

            Vec3 unit_direction = unit_vector(r.direction());
            auto a = 0.5 * (unit_direction.y() + 1.0);
            return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
        };

        out << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++)
        {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' '
                      << std::flush;
            for (int i = 0; i < image_width; i++)
            {
                color pixel_color(0, 0, 0);
                for (int sample = 0; sample < samples_per_pixel; sample++)
                {
                    ray r = get_ray(i, j);
                    pixel_color += k_ray_color(r, max_depth, world, reflector);
                }
                if (gamma)
                    write_color_with_gamma(out, pixel_samples_scale * pixel_color);
                else
                    write_color(out, pixel_samples_scale * pixel_color);
            }
        }

        std::cout << "\rDone.                 \n";
    }

    void render_metal(const hittable &world, std::ostream &out)
    {
        initialize();

        constexpr auto k_ray_color = [](this auto self, const ray &r, int depth,
                                        const hittable &world) {
            // If we've exceeded the ray bounce limit, no more light is gathered.
            if (depth <= 0)
                return color(0, 0, 0);

            hit_record rec;

            if (world.hit(r, interval(0.001, infinity), rec))
            {
                // NOTE: 使用材质渲染
                ray scattered;
                color attenuation;
                if (rec.mat->scatter(r, rec, attenuation, scattered))
                    return attenuation * self(scattered, depth - 1, world);
                return color(0, 0, 0);
            }

            Vec3 unit_direction = unit_vector(r.direction());
            auto a = 0.5 * (unit_direction.y() + 1.0);
            return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
        };

        out << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++)
        {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' '
                      << std::flush;
            for (int i = 0; i < image_width; i++)
            {
                color pixel_color(0, 0, 0);
                for (int sample = 0; sample < samples_per_pixel; sample++)
                {
                    ray r = get_ray(i, j);
                    pixel_color += k_ray_color(r, max_depth, world);
                }
                write_color_with_gamma(out, pixel_samples_scale * pixel_color);
            }
        }

        std::cout << "\rDone.                 \n";
    }

  private:
    int image_height;   // Rendered image height
    point3 center;      // Camera center
    point3 pixel00_loc; // Location of pixel 0, 0
    Vec3 pixel_delta_u; // Offset to pixel to the right
    Vec3 pixel_delta_v; // Offset to pixel below

    double pixel_samples_scale; // Color scale factor for a sum of pixel samples

    void initialize()
    {
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        pixel_samples_scale = 1.0 / samples_per_pixel;

        center = point3(0, 0, 0);

        // Determine viewport dimensions.
        auto focal_length = 1.0;
        auto viewport_height = 2.0;
        auto viewport_width = viewport_height * (double(image_width) / image_height);

        // Calculate the vectors across the horizontal and down the vertical viewport
        // edges.
        auto viewport_u = Vec3(viewport_width, 0, 0);
        auto viewport_v = Vec3(0, -viewport_height, 0);

        // Calculate the horizontal and vertical delta vectors from pixel to pixel.
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        // Calculate the location of the upper left pixel.
        auto viewport_upper_left =
            center - Vec3(0, 0, focal_length) - viewport_u / 2 - viewport_v / 2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
    }

    [[nodiscard]] color ray_color(const ray &r, const hittable &world,
                                  bool diffuse = false) const
    {
        hit_record rec;

        if (world.hit(r, interval(0, infinity), rec))
        {

            if (not diffuse)
                return 0.5 * (rec.normal + color(1, 1, 1));

            /*
如果光线从材料上反弹并保持100%的颜色，那么我们说该材料是白色的。
如果光线从材料上反弹并保持0%的颜色，那么我们说该材料是黑色的。
作为我们新漫射材料的第一个演示，我们将设置ray_color函数从反弹中返回50%的颜色。
我们应该期望得到一个漂亮的灰色。
//NOTE: ray_color 是递归的，要有明确的终止，而不是等待hit失败
*/
            Vec3 direction = random_on_hemisphere(rec.normal);
            return 0.5 * ray_color(ray(rec.p, direction), world, diffuse);
        }

        Vec3 unit_direction = unit_vector(r.direction());
        auto a = 0.5 * (unit_direction.y() + 1.0);
        return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
    }

    [[nodiscard]] color ray_color(const ray &r, int depth, const hittable &world,
                                  bool lambertian = true) const
    {
        // 如果我们超过了光线反射极限，就不会再聚集光线。
        //  If we've exceeded the ray bounce limit, no more light is gathered.
        if (depth <= 0)
            return {0, 0, 0};

        hit_record rec;

        // NOTE: bug:  if (world.hit(r, interval(0, infinity), rec))。 会自相交
        /*
        interval(0.001, infinity) 的关键作用：
        避免自相交：跳过光线起点附近的无效交点
        解决数值精度：补偿浮点数舍入误差
        保持物理正确：不影响正常的物体相交检测
        提高稳定性：防止无限递归和渲染artifacts

        这是光线追踪中的经典技巧，被称为"shadow bias"或"ray epsilon"！
        */
        if (world.hit(r, interval(0.001, infinity), rec))
        {
            Vec3 direction = lambertian ? rec.normal + random_unit_vector()
                                        : random_on_hemisphere(rec.normal);
            // 每次depth - 1
            return 0.5 * ray_color(ray(rec.p, direction), depth - 1, world);
        }

        Vec3 unit_direction = unit_vector(r.direction());
        auto a = 0.5 * (unit_direction.y() + 1.0);
        return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
    }

    [[nodiscard]] ray get_ray(int i, int j) const
    {
        // Construct a camera ray originating from the origin and directed at randomly
        // sampled point around the pixel location i, j.
        // 构造一个来自原点并指向随机采样的相机光线点周围的像素位置i， j。
        auto offset = sample_square();
        auto pixel_sample = pixel00_loc + ((i + offset.x()) * pixel_delta_u) +
                            ((j + offset.y()) * pixel_delta_v);

        auto ray_origin = center;
        auto ray_direction = pixel_sample - ray_origin;

        return ray(ray_origin, ray_direction);
    }
    [[nodiscard]] Vec3 sample_square() const
    {
        // 将向量返回到[-.5，-.5]-[+.5，+.5]单位平方中的随机点。
        //  Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
        return {random_double() - 0.5, random_double() - 0.5, 0};
    }
};