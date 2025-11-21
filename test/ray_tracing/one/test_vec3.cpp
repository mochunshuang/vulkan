#include "color.h"
#include "vec3.h"

#include <fstream>

// NOLINTBEGIN
static constexpr void generate_ppm_with_vec3(const std::string &filename, int width,
                                             int height)
{
    std::ofstream file(filename, std::ios::out);

    // PPM 文件头
    file << "P3\n" << width << " " << height << "\n255\n";

    // 生成渐变图像
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            // 使用 Vec3 表示颜色
            auto x = static_cast<double>(i) / (width - 1);
            auto g = static_cast<double>(j) / (height - 1);
            double b = 0;

            write_color(file, Vec3{x, g, b});
        }
    }
}
void generate_random_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            // 使用 Vec3::random() 生成随机颜色
            color pixel_color = Vec3::random();
            write_color(file, pixel_color);
        }
    }
}

void generate_ppm_with_b(const std::string &filename, int width, int height, double b)
{
    std::ofstream file(filename);

    // PPM 文件头
    file << "P3\n" << width << " " << height << "\n255\n";

    // 生成渐变图像
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            // 使用 Vec3 表示颜色
            auto r = static_cast<double>(i) / (width - 1);
            auto g = static_cast<double>(j) / (height - 1);

            color pixel_color(r, g, b);
            write_color(file, pixel_color);
        }
    }
}

// NOTE: 实现渐变
void generate_gradient_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            // 使用 Vec3 的向量运算
            color top_color(1.0, 0.0, 0.0);    // 红色
            color bottom_color(0.0, 0.0, 1.0); // 蓝色

            double blend = static_cast<double>(j) / (height - 1);
            color pixel_color = (1.0 - blend) * top_color + blend * bottom_color;

            write_color(file, pixel_color);
        }
    }
}
void generate_diagonal_gradient_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            // 左上角到右下角渐变
            color top_left_color(1.0, 0.0, 0.0);     // 红色（左上）
            color bottom_right_color(0.0, 0.0, 1.0); // 蓝色（右下）

            // 对角线混合：同时考虑 x 和 y
            double blend_x = static_cast<double>(i) / (width - 1);
            double blend_y = static_cast<double>(j) / (height - 1);
            double blend = (blend_x + blend_y) / 2.0; // 对角线混合

            color pixel_color =
                (1.0 - blend) * top_left_color + blend * bottom_right_color;

            write_color(file, pixel_color);
        }
    }
}

void generate_exact_diagonal_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            color top_left(1.0, 0.0, 0.0);     // 红色
            color bottom_right(0.0, 0.0, 1.0); // 蓝色

            // 计算到对角线的距离
            double diagonal_progress = static_cast<double>(i + j) / (width + height - 2);

            color pixel_color =
                (1.0 - diagonal_progress) * top_left + diagonal_progress * bottom_right;
            write_color(file, pixel_color);
        }
    }
}

void generate_45degree_diagonal_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            color top_left(1.0, 0.0, 0.0);     // 红色
            color bottom_right(0.0, 0.0, 1.0); // 蓝色

            // 45度对角线：y = x 的投影
            double diagonal = (static_cast<double>(i) / (width - 1)) +
                              (static_cast<double>(j) / (height - 1));
            double blend = diagonal / 2.0; // 归一化到 [0,1]

            color pixel_color = (1.0 - blend) * top_left + blend * bottom_right;
            write_color(file, pixel_color);
        }
    }
}

void generate_multi_color_diagonal_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            // 多个颜色点
            color top_left(1.0, 0.0, 0.0);     // 红色
            color center(0.0, 1.0, 0.0);       // 绿色
            color bottom_right(0.0, 0.0, 1.0); // 蓝色

            double progress = static_cast<double>(i + j) / (width + height - 2);

            color pixel_color;
            if (progress < 0.5)
            {
                // 红色到绿色
                double blend = progress * 2.0;
                pixel_color = (1.0 - blend) * top_left + blend * center;
            }
            else
            {
                // 绿色到蓝色
                double blend = (progress - 0.5) * 2.0;
                pixel_color = (1.0 - blend) * center + blend * bottom_right;
            }

            write_color(file, pixel_color);
        }
    }
}

double fade(double t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}
double lerp(double t, double a, double b)
{
    return a + t * (b - a);
}
double grad(int hash, double x, double y, double z)
{
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

void generate_polar_waves_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    double center_x = width / 2.0;
    double center_y = height / 2.0;
    double max_dist = std::sqrt(center_x * center_x + center_y * center_y);

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            double dx = i - center_x;
            double dy = j - center_y;
            double dist = std::sqrt(dx * dx + dy * dy);
            double angle = std::atan2(dy, dx);

            // 多个波纹叠加
            double pattern1 = 0.5 + 0.5 * std::sin(dist * 0.1 + angle * 5);
            double pattern2 = 0.5 + 0.5 * std::sin(dist * 0.05 - angle * 3);
            double pattern3 = 0.5 + 0.5 * std::sin(angle * 8 + dist * 0.02);

            double r = pattern1;
            double g = pattern2;
            double b = pattern3;

            color pixel_color(r, g, b);
            write_color(file, pixel_color);
        }
    }
}
void generate_galaxy_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    double center_x = width / 2.0;
    double center_y = height / 2.0;

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            double dx = i - center_x;
            double dy = j - center_y;
            double dist = std::sqrt(dx * dx + dy * dy);
            double angle = std::atan2(dy, dx);

            // 漩涡效果
            double spiral = std::sin(angle * 5 + std::log(dist + 1.0) * 3);
            double arms = std::pow(std::abs(spiral), 0.5);

            // 中心亮，边缘暗
            double falloff = 1.0 - std::min(dist / (width / 2.0), 1.0);

            double brightness = arms * falloff;

            // 星系颜色：中心偏黄，边缘偏蓝
            double r = brightness * (1.0 - dist / (width / 2.0) * 0.5);
            double g = brightness * (1.0 - dist / (width / 2.0) * 0.3);
            double b = brightness * (0.5 + dist / (width / 2.0) * 0.5);

            color pixel_color(r, g, b);
            write_color(file, pixel_color);
        }
    }
}
void generate_domain_warp_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            double x = (i - width / 2.0) / width * 10.0;
            double y = (j - height / 2.0) / height * 10.0;

            // 域扭曲
            for (int k = 0; k < 3; k++)
            {
                double nx = std::sin(x) + std::cos(y);
                double ny = std::cos(x) - std::sin(y);
                x = nx;
                y = ny;
            }

            double r = 0.5 + 0.5 * std::sin(x * 5.0);
            double g = 0.5 + 0.5 * std::sin(y * 5.0);
            double b = 0.5 + 0.5 * std::sin((x + y) * 3.0);

            color pixel_color(r, g, b);
            write_color(file, pixel_color);
        }
    }
}

void generate_sierpinski_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    // 使用一维数组，性能更好
    std::vector<uint8_t> canvas(width * height, 0);

    auto draw_triangle = [&](this auto &&self, int x, int y, int size,
                             int depth) -> void {
        if (depth <= 0 || size < 2)
            return;

        // 绘制当前三角形
        for (int i = 0; i < size; ++i)
        {
            for (int j = 0; j <= i; ++j)
            {
                int px = x - i / 2 + j;
                int py = y + i;
                if (py < height && px < width && px >= 0)
                {
                    canvas[py * width + px] = 1;
                }
            }
        }

        int new_size = size / 2;
        self(x, y, new_size, depth - 1);
        self(x - new_size / 2, y + new_size, new_size, depth - 1);
        self(x + new_size / 2, y + new_size, new_size, depth - 1);
    };

    draw_triangle(width / 2, 50, 400, 5);

    // 输出到PPM
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            if (canvas[j * width + i])
            {
                file << "255 255 255\n";
            }
            else
            {
                file << "0 0 0\n";
            }
        }
    }
}

constexpr auto M_PI = 3.1415926;

void generate_fractal_tree_ppm(const std::string &filename, int width, int height)
{

    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    std::vector<std::vector<color>> canvas(
        height, std::vector<color>(width, color(0.1, 0.1, 0.3))); // 深蓝背景

    auto draw_branch = [&](this auto &&self, double x, double y, double angle,
                           double length, int depth, const color &col) -> void {
        if (depth <= 0 || length < 2)
            return;

        double x2 = x + length * std::cos(angle);
        double y2 = y + length * std::sin(angle);

        // 绘制树枝（抗锯齿简化版）
        int steps = std::max(1, static_cast<int>(length));
        for (int i = 0; i < steps; ++i)
        {
            double t = static_cast<double>(i) / steps;
            double cx = x + (x2 - x) * t;
            double cy = y + (y2 - y) * t;

            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    int ix = static_cast<int>(cx + dx);
                    int iy = static_cast<int>(cy + dy);
                    if (ix >= 0 && ix < width && iy >= 0 && iy < height)
                    {
                        // 混合颜色
                        double weight = 1.0 - (std::abs(dx) + std::abs(dy)) * 0.2;
                        canvas[iy][ix] = canvas[iy][ix] + col * weight;
                    }
                }
            }
        }

        if (depth > 1)
        {
            // 随机分支角度和长度
            double angle_var = M_PI / 6 * (0.8 + 0.4 * std::sin(depth * 2.0));
            double length_ratio = 0.65 + 0.1 * std::cos(depth * 3.0);

            color new_col = col * color(1.1, 1.05, 0.95); // 颜色渐变

            self(x2, y2, angle - angle_var, length * length_ratio, depth - 1, new_col);
            self(x2, y2, angle + angle_var, length * length_ratio, depth - 1, new_col);

            // 30%几率生成第三个分支
            if (depth > 3 && std::sin(depth * 5.0) > 0.7)
            {
                self(x2, y2, angle, length * 0.8, depth - 2, col * 0.8);
            }
        }
    };

    // 绘制多棵树形成森林
    draw_branch(width * 0.3, height - 50, -M_PI / 2, 80, 12, color(0.6, 0.4, 0.2));
    draw_branch(width * 0.7, height - 50, -M_PI / 2, 100, 14, color(0.5, 0.3, 0.15));
    draw_branch(width * 0.5, height - 30, -M_PI / 2, 60, 10, color(0.7, 0.5, 0.25));

    // 输出
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            write_color(file, canvas[j][i]);
        }
    }
}

void generate_beautiful_mandelbrot_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    // 选择曼德勃罗集的美丽区域
    double center_x = -0.7, center_y = 0.0;
    double scale = 2.4;

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            double x0 = (i - width * 0.5) * scale / width + center_x;
            double y0 = (j - height * 0.5) * scale / height + center_y;

            double x = 0.0, y = 0.0;
            int iteration = 0;
            const int max_iterations = 100;

            // 曼德勃罗迭代
            while (x * x + y * y <= 4.0 && iteration < max_iterations)
            {
                double x_temp = x * x - y * y + x0;
                y = 2.0 * x * y + y0;
                x = x_temp;
                iteration++;
            }

            // 平滑的色彩映射
            if (iteration < max_iterations)
            {
                double log_zn = std::log(x * x + y * y) / 2.0;
                double nu = std::log(log_zn / std::log(2.0)) / std::log(2.0);
                double smooth_iter = iteration + 1 - nu;

                // 美丽的色彩渐变
                double t = smooth_iter / max_iterations;
                double r = 0.5 + 0.5 * std::cos(3.0 + t * 15.0);
                double g = 0.5 + 0.5 * std::cos(3.0 + t * 13.0 + 2.0);
                double b = 0.5 + 0.5 * std::cos(3.0 + t * 11.0 + 4.0);

                // 增强色彩饱和度
                r = std::pow(r, 0.6);
                g = std::pow(g, 0.7);
                b = std::pow(b, 0.8);

                color pixel_color(r, g, b);
                write_color(file, pixel_color);
            }
            else
            {
                // 集合内部的颜色（深色）
                file << "0 0 0\n";
            }
        }
    }
}

void generate_recursive_circles_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    std::vector<std::vector<color>> canvas(height,
                                           std::vector<color>(width, color(0, 0, 0)));

    auto draw_circles = [&](this auto &&self, double x, double y, double radius,
                            int depth, const color &col) -> void {
        if (depth <= 0 || radius < 2)
            return;

        // 绘制当前圆
        int r_int = static_cast<int>(radius);
        for (int j = -r_int; j <= r_int; ++j)
        {
            for (int i = -r_int; i <= r_int; i++)
            {
                double dist = std::sqrt(i * i + j * j);
                if (std::abs(dist - radius) < 1.5)
                { // 圆环
                    int px = static_cast<int>(x + i);
                    int py = static_cast<int>(y + j);
                    if (px >= 0 && px < width && py >= 0 && py < height)
                    {
                        double blend = 1.0 - std::abs(dist - radius);
                        canvas[py][px] = canvas[py][px] + col * blend;
                    }
                }
            }
        }

        // 递归绘制小圆
        double angle_step = 2.0 * M_PI / 6; // 6个小圆
        for (int k = 0; k < 6; ++k)
        {
            double angle = angle_step * k;
            double new_x = x + (radius * 0.6) * std::cos(angle);
            double new_y = y + (radius * 0.6) * std::sin(angle);

            color new_col = col * color(0.9, 0.95, 1.1); // 颜色变化
            self(new_x, new_y, radius * 0.4, depth - 1, new_col);
        }

        // 中心再画一个
        if (depth > 2)
        {
            self(x, y, radius * 0.3, depth - 2, col * 1.2);
        }
    };

    // 绘制多个递归圆形组
    draw_circles(width * 0.3, height * 0.3, 80, 5, color(1.0, 0.3, 0.3));
    draw_circles(width * 0.7, height * 0.3, 70, 4, color(0.3, 1.0, 0.3));
    draw_circles(width * 0.5, height * 0.7, 90, 6, color(0.3, 0.3, 1.0));
    draw_circles(width * 0.2, height * 0.7, 60, 4, color(1.0, 1.0, 0.3));

    // 输出
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            write_color(file, canvas[j][i]);
        }
    }
}

void generate_julia_set_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    // 茱莉亚集的美丽参数
    double c_real = -0.7;
    double c_imag = 0.27015;

    double scale = 3.0;

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            double zx = (i - width * 0.5) * scale / width;
            double zy = (j - height * 0.5) * scale / height;

            int iteration = 0;
            const int max_iterations = 300;

            // 茱莉亚集迭代
            while (zx * zx + zy * zy <= 4.0 && iteration < max_iterations)
            {
                double x_temp = zx * zx - zy * zy + c_real;
                zy = 2.0 * zx * zy + c_imag;
                zx = x_temp;
                iteration++;
            }

            // 华丽的色彩映射
            if (iteration < max_iterations)
            {
                double t = static_cast<double>(iteration) / max_iterations;

                // 彩虹色渐变
                double r = 0.5 + 0.5 * std::sin(t * 20.0);
                double g = 0.5 + 0.5 * std::sin(t * 20.0 + 2.0 * M_PI / 3.0);
                double b = 0.5 + 0.5 * std::sin(t * 20.0 + 4.0 * M_PI / 3.0);

                // 指数增强色彩
                r = std::pow(r, 0.5);
                g = std::pow(g, 0.6);
                b = std::pow(b, 0.7);

                color pixel_color(r, g, b);
                write_color(file, pixel_color);
            }
            else
            {
                // 集合内部的深蓝色
                file << "10 20 40\n";
            }
        }
    }
}

void generate_nebula_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    std::vector<std::vector<color>> canvas(height,
                                           std::vector<color>(width, color(0, 0, 0)));

    // 生成多个星云团
    auto generate_star_cluster = [&](double center_x, double center_y, double size,
                                     const color &base_color) {
        for (int j = 0; j < height; ++j)
        {
            for (int i = 0; i < width; ++i)
            {
                double dx = (i - center_x) / size;
                double dy = (j - center_y) / size;
                double dist = std::sqrt(dx * dx + dy * dy);

                if (dist < 3.0)
                {
                    // 星云密度分布
                    double density =
                        std::exp(-dist * dist) * (1.0 + 0.5 * std::sin(dist * 8.0));

                    // 颜色变化
                    double r_var = 0.3 * std::sin(i * 0.01 + j * 0.02);
                    double g_var = 0.3 * std::sin(i * 0.015 + j * 0.025);
                    double b_var = 0.3 * std::sin(i * 0.02 + j * 0.03);

                    color nebula_color =
                        base_color * density + color(r_var, g_var, b_var) * 0.2;
                    canvas[j][i] = canvas[j][i] + nebula_color;
                }
            }
        }
    };

    // 添加背景星云
    generate_star_cluster(width * 0.3, height * 0.4, 150,
                          color(0.8, 0.2, 0.4)); // 红色星云
    generate_star_cluster(width * 0.7, height * 0.6, 120,
                          color(0.2, 0.4, 0.8)); // 蓝色星云
    generate_star_cluster(width * 0.5, height * 0.3, 100,
                          color(0.3, 0.8, 0.3)); // 绿色星云

    // 添加星星
    std::mt19937 gen(42);
    std::uniform_real_distribution<double> dist(0, 1);

    for (int star = 0; star < 2000; ++star)
    {
        int x = static_cast<int>(dist(gen) * width);
        int y = static_cast<int>(dist(gen) * height);
        double brightness = 0.5 + dist(gen) * 0.5;

        // 星星大小和颜色
        int star_size = 1 + static_cast<int>(dist(gen) * 3);
        color star_color(1.0, 0.9 + dist(gen) * 0.1, 0.8 + dist(gen) * 0.2);

        for (int dy = -star_size; dy <= star_size; ++dy)
        {
            for (int dx = -star_size; dx <= star_size; ++dx)
            {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < width && py >= 0 && py < height)
                {
                    double falloff =
                        1.0 - (std::abs(dx) + std::abs(dy)) / (2.0 * star_size);
                    canvas[py][px] = canvas[py][px] + star_color * brightness * falloff;
                }
            }
        }
    }

    // 输出
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            write_color(file, canvas[j][i]);
        }
    }
}

void generate_spiral_galaxy_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    double center_x = width / 2.0;
    double center_y = height / 2.0;

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            double dx = i - center_x;
            double dy = j - center_y;
            double dist = std::sqrt(dx * dx + dy * dy);
            double angle = std::atan2(dy, dx);

            // 星系核心亮度
            double core_brightness = std::exp(-dist * dist / (width * 0.1));

            // 螺旋臂
            double spiral_arms = 0.0;
            for (int arm = 0; arm < 4; ++arm)
            { // 4条旋臂
                double arm_angle = angle + arm * M_PI / 2;
                double spiral = std::sin(arm_angle * 4 + std::log(dist + 1.0) * 3);
                spiral_arms +=
                    std::max(0.0, spiral * std::exp(-std::abs(dist - width * 0.3) /
                                                    (width * 0.1)));
            }

            // 整体亮度分布
            double brightness = core_brightness + spiral_arms * 0.3;

            // 星系颜色：核心偏黄，旋臂偏蓝
            double age_factor = std::min(dist / (width * 0.4), 1.0);
            double r = brightness * (1.0 - age_factor * 0.7);
            double g = brightness * (1.0 - age_factor * 0.5);
            double b = brightness * (0.6 + age_factor * 0.4);

            // 添加随机星点
            double star_noise = std::sin(i * 123.45 + j * 67.89) * 0.1;
            brightness += std::max(0.0, star_noise);

            color pixel_color(r, g, b);
            write_color(file, pixel_color);
        }
    }
}

void generate_supernova_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    double center_x = width / 2.0;
    double center_y = height / 2.0;

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            double dx = i - center_x;
            double dy = j - center_y;
            double dist = std::sqrt(dx * dx + dy * dy);

            // 超新星冲击波
            double shockwave_radius = width * 0.3;
            double shockwave =
                std::exp(-std::abs(dist - shockwave_radius) / (width * 0.05));

            // 核心亮度
            double core = std::exp(-dist * dist / (width * 0.05));

            // 辐射条纹
            double angle = std::atan2(dy, dx);
            double rays = std::pow(std::abs(std::sin(angle * 12)), 0.3) *
                          std::exp(-dist / (width * 0.4));

            // 总亮度
            double brightness = core * 2.0 + shockwave * 0.8 + rays * 0.6;

            // 颜色：核心白色，冲击波蓝色，辐射金色
            double r = core * 1.0 + shockwave * 0.3 + rays * 1.0;
            double g = core * 0.9 + shockwave * 0.5 + rays * 0.8;
            double b = core * 0.8 + shockwave * 1.0 + rays * 0.3;

            color pixel_color(r, g, b);
            write_color(file, pixel_color);
        }
    }
}

void generate_deep_space_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    std::vector<std::vector<color>> canvas(
        height, std::vector<color>(width, color(0.02, 0.03, 0.06)));

    // 生成明显的尘埃云
    auto generate_dust_cloud = [&](double center_x, double center_y, double size,
                                   const color &dust_color, double intensity) {
        for (int j = 0; j < height; ++j)
        {
            for (int i = 0; i < width; ++i)
            {
                double dx = (i - center_x) / size;
                double dy = (j - center_y) / size;
                double dist = dx * dx + dy * dy; // 用平方距离更快

                if (dist < 9.0)
                { // 3.0的平方
                    // 使用多层噪声创建真实的尘埃云形状
                    double noise1 = std::sin(i * 0.02 + j * 0.015) * 0.5 + 0.5;
                    double noise2 = std::sin(i * 0.05 + j * 0.04) * 0.3 + 0.3;
                    double noise3 = std::sin(i * 0.1 + j * 0.08) * 0.2 + 0.2;

                    double density =
                        std::exp(-dist) * (noise1 + noise2 + noise3) * intensity;

                    // 确保尘埃云可见
                    if (density > 0.1)
                    {
                        canvas[j][i] = canvas[j][i] + dust_color * density;
                    }
                }
            }
        }
    };

    // 生成明显的遥远星系
    auto add_distant_galaxy = [&](double x, double y, double size,
                                  const color &galaxy_color) {
        for (int j = std::max(0, (int)(y - size * 2));
             j < std::min(height, (int)(y + size * 2)); ++j)
        {
            for (int i = std::max(0, (int)(x - size * 2));
                 i < std::min(width, (int)(x + size * 2)); ++i)
            {
                double dx = (i - x) / size;
                double dy = (j - y) / size;
                double dist = std::sqrt(dx * dx + dy * dy);

                if (dist < 2.0)
                {
                    // 星系的核心和晕轮
                    double core = std::exp(-dist * dist * 4.0) * 0.8; // 明亮核心
                    double halo = std::exp(-dist * 2.0) * 0.4;        // 扩散晕轮
                    double brightness = core + halo;

                    // 添加一些结构
                    double structure = std::sin(dx * 8.0) * std::cos(dy * 8.0) * 0.1;
                    brightness += std::max(0.0, structure);

                    canvas[j][i] = canvas[j][i] + galaxy_color * brightness;
                }
            }
        }
    };

    // 添加明显的尘埃云（更大、更亮）
    generate_dust_cloud(width * 0.2, height * 0.7, 120, color(0.4, 0.3, 0.6),
                        0.8); // 紫色尘埃云
    generate_dust_cloud(width * 0.8, height * 0.3, 100, color(0.6, 0.4, 0.3),
                        0.7); // 橙色尘埃云
    generate_dust_cloud(width * 0.6, height * 0.8, 150, color(0.3, 0.5, 0.4),
                        0.6); // 绿色尘埃云
    generate_dust_cloud(width * 0.3, height * 0.2, 80, color(0.5, 0.3, 0.5),
                        0.5); // 粉色尘埃云

    // 添加明显的遥远星系（更大、更亮）
    add_distant_galaxy(width * 0.1, height * 0.1, 15, color(0.8, 0.7, 0.9)); // 蓝紫色星系
    add_distant_galaxy(width * 0.9, height * 0.9, 12, color(0.9, 0.8, 0.6)); // 金黄色星系
    add_distant_galaxy(width * 0.1, height * 0.9, 10, color(0.6, 0.9, 0.8)); // 青绿色星系
    add_distant_galaxy(width * 0.9, height * 0.1, 18, color(0.8, 0.6, 0.9)); // 紫红色星系

    // 添加一些中等大小的星系
    add_distant_galaxy(width * 0.3, height * 0.5, 8, color(0.7, 0.9, 1.0));
    add_distant_galaxy(width * 0.7, height * 0.5, 6, color(1.0, 0.9, 0.7));

    // 添加星星背景（更多更亮的星星）
    std::mt19937 gen(42);
    std::uniform_real_distribution<double> dist(0, 1);

    for (int star = 0; star < 5000; ++star)
    {
        int x = static_cast<int>(dist(gen) * width);
        int y = static_cast<int>(dist(gen) * height);

        // 不同大小的星星
        double star_size = 0.5 + dist(gen) * 2.5;
        double brightness = 0.3 + dist(gen) * 0.7;

        // 星星颜色变化
        color star_color(1.0, 0.95 + dist(gen) * 0.05, 0.9 + dist(gen) * 0.1);

        for (int dy = -2; dy <= 2; ++dy)
        {
            for (int dx = -2; dx <= 2; ++dx)
            {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < width && py >= 0 && py < height)
                {
                    double pixel_dist = std::sqrt(dx * dx + dy * dy);
                    if (pixel_dist <= star_size)
                    {
                        double falloff = 1.0 - pixel_dist / star_size;
                        canvas[py][px] =
                            canvas[py][px] + star_color * brightness * falloff;
                    }
                }
            }
        }
    }

    // 输出
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            write_color(file, canvas[j][i]);
        }
    }
}

void generate_simple_deep_space_ppm(const std::string &filename, int width, int height)
{
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";

    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            color pixel_color(0.02, 0.03, 0.06); // 深空背景

            // 明显的尘埃云1（左下角）
            double dx1 = i - width * 0.2;
            double dy1 = j - height * 0.7;
            double dist1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
            if (dist1 < 100)
            {
                double density = std::exp(-dist1 / 50.0) * 0.6;
                pixel_color = pixel_color + color(0.4, 0.3, 0.6) * density;
            }

            // 明显的尘埃云2（右上角）
            double dx2 = i - width * 0.8;
            double dy2 = j - height * 0.3;
            double dist2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
            if (dist2 < 80)
            {
                double density = std::exp(-dist2 / 40.0) * 0.5;
                pixel_color = pixel_color + color(0.6, 0.4, 0.3) * density;
            }

            // 明显的星系1（左上角）
            double dx3 = i - width * 0.1;
            double dy3 = j - height * 0.1;
            double dist3 = std::sqrt(dx3 * dx3 + dy3 * dy3);
            if (dist3 < 20)
            {
                double brightness = std::exp(-dist3 / 8.0) * 0.8;
                pixel_color = pixel_color + color(0.8, 0.7, 0.9) * brightness;
            }

            // 明显的星系2（右下角）
            double dx4 = i - width * 0.9;
            double dy4 = j - height * 0.9;
            double dist4 = std::sqrt(dx4 * dx4 + dy4 * dy4);
            if (dist4 < 15)
            {
                double brightness = std::exp(-dist4 / 6.0) * 0.7;
                pixel_color = pixel_color + color(0.9, 0.8, 0.6) * brightness;
            }

            // 随机星星
            double star = std::sin(i * 123.456 + j * 78.9) * 0.5 + 0.5;
            if (star > 0.95)
            {
                pixel_color = pixel_color + color(1.0, 1.0, 1.0) * (star - 0.95) * 10.0;
            }

            write_color(file, pixel_color);
        }
    }
}

int main()
{

    // Image

    constexpr int image_width = 256;  // NOLINT
    constexpr int image_height = 256; // NOLINT

    // Render

    generate_ppm_with_vec3("vec3_image.ppm", image_width, image_width);
    generate_random_ppm("vec3_random_image.ppm", image_width, image_width);
    generate_gradient_ppm("vec3_blend_image.ppm", image_width, image_width);

    generate_ppm_with_b("vec3_image2.ppm", image_width, image_width,
                        0.25); // NOLINT

    generate_diagonal_gradient_ppm("vec3_top_left_bottom_right.ppm", image_width,
                                   image_width);
    generate_exact_diagonal_ppm("vec3_top_left_bottom_right2.ppm", image_width,
                                image_width);
    generate_45degree_diagonal_ppm("vec3_top_left_bottom_right3.ppm", image_width,
                                   image_width);
    // generate_multi_color_diagonal_ppm
    generate_multi_color_diagonal_ppm("vec3_top_left_bottom_right4.ppm", image_width,
                                      image_width);

    generate_polar_waves_ppm("vec3_polar_waves.ppm", image_width, image_width);

    generate_galaxy_ppm("vec3_galaxy.ppm", image_width, image_width);

    generate_domain_warp_ppm("vec3_domain_warp.ppm", image_width, image_width);

    generate_sierpinski_ppm("vec3_sierpinski.ppm", image_width, image_width);
    // generate_fractal_tree_ppm
    generate_fractal_tree_ppm("vec3_fractal_tree.ppm", image_width, image_width);
    generate_beautiful_mandelbrot_ppm("vec3_mandelbrot2.ppm", image_width, image_width);
    generate_recursive_circles_ppm("vec3_circles.ppm", image_width, image_width);
    generate_julia_set_ppm("vec3_julia_set.ppm", image_width, image_width);

    generate_nebula_ppm("vec3_nebula.ppm", image_width, image_width);
    generate_spiral_galaxy_ppm("vec3_spiral_galaxy.ppm", image_width, image_width);
    generate_supernova_ppm("vec3_supernova.ppm", image_width, image_width);
    generate_deep_space_ppm("vec3_deep_space.ppm", image_width, image_width);
    generate_simple_deep_space_ppm("vec3_deep_space2.ppm", image_width, image_width);

    // NOTE: 观察 ppm 的RGB值
    std::clog << "\rDone.n";
}
// NOLINTEND