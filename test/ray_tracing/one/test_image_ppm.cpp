#include <iostream>
#include <fstream>
#include <limits>
#include <vector>

// NOLINTBEGIN
// 打印文件内容（前几行）
void printFileContent(const std::string &filename, int lines = 10)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cout << "无法打开文件: " << filename << std::endl;
        return;
    }

    std::cout << "\n=== " << filename << " 文件内容（前" << lines
              << "行）===" << std::endl;
    std::string line;
    for (int i = 0; i < lines && std::getline(file, line); i++)
    {
        std::cout << line << std::endl;
    }
    std::cout << "=== 内容结束 ===" << std::endl;
}

// 打印文件统计信息
void printFileStats(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cout << "无法打开文件: " << filename << std::endl;
        return;
    }

    auto size = file.tellg();
    file.close();

    std::cout << "📊 文件统计: " << filename << std::endl;
    std::cout << "   大小: " << size << " 字节" << std::endl;
    std::cout << "   状态: " << (size > 0 ? "✅ 正常" : "❌ 异常") << std::endl;
}
void gen_ppm(const std::string &filename, size_t width, size_t height,
             bool binaryFormat = false)
{
    auto mode = binaryFormat ? std::ios::binary : std::ios::out;
    std::ofstream file(filename, mode);

    // 写入文件头
    file << (binaryFormat ? "P6" : "P3") << '\n' << width << ' ' << height << "\n255\n";

    // 写入像素数据
    for (size_t y = 0; y < height; y++)
    {
        for (size_t x = 0; x < width; x++)
        {
            unsigned char r = (x * 255) / (width - 1);
            unsigned char g = (y * 255) / (height - 1);
            unsigned char b = 128;

            if (binaryFormat)
            {
                file.put(r);
                file.put(g);
                file.put(b);
            }
            else
            {
                file << static_cast<int>(r) << ' ' << static_cast<int>(g) << ' '
                     << static_cast<int>(b) << '\n';
            }
        }
    }
    // file 自动关闭
}

constexpr void test_gen_ppm()
{
    const size_t width = 256;
    const size_t height = 256;

    // 最简洁的调用方式
    gen_ppm("binary.ppm", width, height, true); // P6
    gen_ppm("ascii.ppm", width, height, false); // P3

    printFileContent("ascii.ppm");
    std::cout << "文件生成完成！" << '\n';
}

struct rgb
{
    uint8_t r, g, b;
};

class PPMGenerator
{
  private:
    std::vector<rgb> pixels_;
    size_t width_, height_;

  public:
    PPMGenerator(size_t width, size_t height) : width_(width), height_(height)
    {
        pixels_.resize(width * height);
    }

    void setPixel(size_t x, size_t y, rgb color)
    {
        pixels_[(y * width_) + x] = color;
    }

    void generateGradient()
    {
        /*

        输入值	int(255.999 * x)	std::round(255.0 * x)
        0.999	255	                255 ✓
        0.5	    127	                128 ✓
        0.499	127	                127 ✓
        */
        static_assert((std::numeric_limits<uint8_t>::max)() == 255);
        for (size_t y = 0; y < height_; y++)
        {
            for (size_t x = 0; x < width_; x++)
            {
                auto r = double(x) / (width_ - 1);
                auto g = double(y) / (height_ - 1);
                auto b = 0.0;
                pixels_[(y * width_) + x] = {
                    .r = static_cast<uint8_t>(std::round(255 * r)),
                    .g = static_cast<uint8_t>(std::round(255 * g)),
                    .b = static_cast<uint8_t>(b)};
            }
        }
    }

    // 一次批量写入所有数据
    void saveToFile(const std::string &filename, bool binaryFormat = false) const
    {
        std::ofstream file(filename, binaryFormat ? std::ios::binary : std::ios::out);

        file << (binaryFormat ? "P6" : "P3") << '\n'
             << width_ << ' ' << height_ << "\n255\n";

        if (binaryFormat)
        {
            // 一次系统调用写入所有数据
            file.write(std::bit_cast<const char *>(pixels_.data()),
                       pixels_.size() * sizeof(rgb));
        }
        else
        {
            // NOTE: 转为int 才打印出数字。 0-255的值可能是控制字符
            for (const auto &pixel : pixels_)
            {
                file << static_cast<int>(pixel.r) << " " << static_cast<int>(pixel.g)
                     << " " << static_cast<int>(pixel.b) << "\n";
            }
        }
    }
};

constexpr void test_optimized_ppm()
{
    const size_t width = 256;
    const size_t height = 256;

    PPMGenerator generator(width, height);
    generator.generateGradient();
    generator.saveToFile("optimized_binary.ppm", true);
    generator.saveToFile("optimized_ascii.ppm", false);

    printFileContent("optimized_ascii.ppm");
    std::cout << "优化版文件生成完成！" << '\n';
}

int main()
{
    test_gen_ppm();
    test_optimized_ppm();
    return 0;
}
// NOLINTEND