#pragma once

#include <memory>
#include <utility>

#include "color.hpp"

#include "perlin.hpp"
#include "rtw_image.hpp"

/*
NOTE: 计算机图形学中的纹理映射是将材料效果应用于场景中对象的过程。
“纹理”部分是效果，“映射”部分是数学意义上的将一个空间映射到另一个空间。
这种效果可以是任何材料属性：颜色、光泽、凹凸几何（称为凹凸映射），甚至是材料存在（创建表面的切口区域）

最常见的纹理映射类型将图像映射到对象表面，定义对象表面上每个点的颜色。
NOTE: 在实践中，我们反过来实现这个过程：给定对象上的某个点，我们将查找纹理映射定义的颜色。

为了执行纹理查找，我们需要一个纹理坐标。这个坐标可以用多种方式定义，随着我们的进步，我们将发展这个想法。现在，我们将传入二维纹理坐标。
按照惯例，纹理坐标命名为u和v。

*/
class vulkan_texture // NOLINT
{
  public:
    virtual ~vulkan_texture() = default;

    [[nodiscard]] virtual color value(double u, double v, const point3 &p) const = 0;
};

class solid_color : public vulkan_texture
{
  public:
    explicit solid_color(const color &albedo) : albedo_(albedo) {}

    solid_color(double red, double green, double blue)
        : solid_color(color(red, green, blue))
    {
    }

    [[nodiscard]] color value(double u, double v, const point3 &p) const override
    {
        return albedo_;
    }

  private:
    color albedo_;
};

// 这是一个棋盘格纹理的实现
class checker_texture : public vulkan_texture // NOLINT
{
  public:
    // 方式1：用现成的纹理对象
    checker_texture(double scale, std::shared_ptr<vulkan_texture> even,
                    std::shared_ptr<vulkan_texture> odd)
        : invScale_(1.0 / scale), even_(std::move(even)), odd_(std::move(odd))
    {
    }
    // 方式2：用纯色创建纹理
    checker_texture(double scale, const color &c1, const color &c2)
        : checker_texture(scale, std::make_shared<solid_color>(c1),
                          std::make_shared<solid_color>(c2))
    {
    }

    /*
    格子坐标 (0,0,0): 0+0+0=0 → 偶数 → 红色
    格子坐标 (1,0,0): 1+0+0=1 → 奇数 → 蓝色
    格子坐标 (0,1,0): 0+1+0=1 → 奇数 → 蓝色
    格子坐标 (1,1,0): 1+1+0=2 → 偶数 → 红色
    格子坐标 (0,0,1): 0+0+1=1 → 奇数 → 蓝色

    2D简化版（只看X-Y平面）：
    Y轴
    ↑
    │ 🟥🟦🟥🟦  // 🟥=红色格 🟦=蓝色格
    │ 🟦🟥🟦🟥
    │ 🟥🟦🟥🟦
    │ 🟦🟥🟦🟥
    └──────────→ X轴
    */
    [[nodiscard]] color value(double u, double v, const point3 &p) const override
    {
        // 核心算法：判断当前位置是偶数格还是奇数格
        auto xInteger = static_cast<int>(std::floor(invScale_ * p.x()));
        auto yInteger = static_cast<int>(std::floor(invScale_ * p.y()));
        auto zInteger = static_cast<int>(std::floor(invScale_ * p.z()));

        bool isEven = (xInteger + yInteger + zInteger) % 2 == 0;

        return isEven ? even_->value(u, v, p) : odd_->value(u, v, p);
    }

  private:
    /*
    scale：棋盘格大小（缩放因子）
    even / odd：偶数格和奇数格的纹理
    invScale_：1.0/scale，为了计算效率
    */
    double invScale_;
    std::shared_ptr<vulkan_texture> even_;
    std::shared_ptr<vulkan_texture> odd_;
};

// NOTE: uv 映射的图像纹理。 uv 的映射需要
class image_texture : public vulkan_texture // NOLINT
{
  public:
    // 构造函数：从图像文件加载纹理
    constexpr explicit image_texture(const char *filename) : image_(filename) {}

    [[nodiscard]] color value(double u, double v, const point3 & /*p*/) const override
    {
        // 如果没有纹理数据，返回青色作为调试辅助
        // If we have no texture data, then return solid cyan as a debugging aid.
        if (image_.height() <= 0)
            return {0, 1, 1};

        // 步骤1：将纹理坐标限制在[0,1]范围内
        // Clamp input texture coordinates to [0,1] x [1,0]
        u = interval(0, 1).clamp(u);

        // 步骤2：翻转V坐标（因为图像坐标系与纹理坐标系Y方向相反）
        // 纹理坐标系：V=0在底部，V=1在顶部
        // 图像坐标系：Y=0在顶部，Y=1在底部
        v = 1.0 - interval(0, 1).clamp(v); // Flip V to image coordinates

        // 步骤3：将归一化的UV坐标转换为像素坐标
        auto i = static_cast<int>(u * image_.width());
        auto j = static_cast<int>(v * image_.height());

        // 步骤4：获取对应像素的RGB数据
        const auto *pixel = image_.pixel_data(i, j);

        // 步骤5：将8位RGB值(0-255)转换为浮点数颜色值(0.0-1.0)
        constexpr auto k_max_value = 255.0;
        auto color_scale = 1.0 / k_max_value;
        return {color_scale * pixel[0], color_scale * pixel[1], color_scale * pixel[2]};
    }

  private:
    rtw_image image_; // 存储图像数据的对象
};

class noise_texture_nosmooth : public vulkan_texture // NOLINT
{
  public:
    noise_texture_nosmooth() = default;

    [[nodiscard]] color value(double u, double v, const point3 &p) const override
    {
        // NOTE: 生成随颜色： 散列随机纹理
        return color(1, 1, 1) * noise_.noise_nosmooth(p);
    }

  private:
    perlin noise_;
};

class noise_texture : public vulkan_texture // NOLINT
{
  public:
    explicit noise_texture(double scale) : scale(scale) {}

    [[nodiscard]] color value(double u, double v, const point3 &p) const override
    {
        // NOTE: 生成随颜色： 散列随机纹理
        return color(1, 1, 1) * noise_.noise(scale * p);
    }

  private:
    perlin noise_;
    double scale;
};

class noise_texture_with_vec : public vulkan_texture // NOLINT
{
  public:
    explicit noise_texture_with_vec(double scale) : scale(scale) {}

    [[nodiscard]] color value(double u, double v, const point3 &p) const override
    {
        /*
        scale * p：控制噪声的频率（缩放采样点）
        1.0 + noise.noise(...)：将范围从[-1,1]映射到[0,2]
        × 0.5：最终映射到[0,1]范围
        color(1,1,1) * ...：产生灰度颜色
        */
        // NOTE: 线性插值
        return color(1, 1, 1) * 0.5 * (1.0 + noise.noise(scale * p));
    }

  private:
    perlin_with_random_vec noise;
    double scale;
};

class noise_texture_with_vec_and_turb : public vulkan_texture // NOLINT
{
  public:
    explicit noise_texture_with_vec_and_turb(double scale) : scale(scale) {}

    [[nodiscard]] color value(double u, double v, const point3 &p) const override
    {
        return color(1, 1, 1) * noise.turb(p, 7);
    }

  private:
    perlin_with_random_vec noise;
    double scale;
};

class noise_texture_with_vec_and_turb_phase : public vulkan_texture // NOLINT
{
  public:
    explicit noise_texture_with_vec_and_turb_phase(double scale) : scale(scale) {}

    /*
🎪 生动比喻：抖动的水面倒影
想象平静水面的正弦波倒影：
  正常正弦波：～～～ ～～～ ～～～（整齐的波浪）
现在用湍流"搅动"水面：
  扰动后：～∿～~~∽~~~∿~~（不规则的自然波浪）

🌊 物理类比：真实大理石的形成
真实大理石是在高温高压下形成的：
  基础层：矿物质沉积的原始层次（对应 scale * p.z()）
  地质活动：地壳运动产生的扭曲力（对应 10 * turb(p, 7)）

💡 为什么这样有效？
因为自然界中的很多图案都是：
  基本规律（正弦波代表的周期性）
  加上随机扰动（湍流代表的不规则性）

这种"规律+扰动"的模式广泛存在于：
  木材的年轮 + 生长变异
  云彩的基本形状 + 大气湍流
  石材的纹理 + 地质变形
*/
    [[nodiscard]] color value(double u, double v, const point3 &p) const override
    {
        // NOTE: std::sin(相位). phase 意思是相位
        // NOTE: 核心思想：用湍流扰动正弦波
        /*
分为两部分：
  1. 基础波形：scale * p.z()  // 沿着Z轴的规则正弦波。效果：产生平行的条纹
  2. 相位扰动：10 * noise.turb(p, 7)  // 湍流噪声乘以放大系数。效果：让条纹弯曲、扭曲
*/
        return color(.5, .5, .5) * (1 + std::sin(scale * p.z() + 10 * noise.turb(p, 7)));
    }

  private:
    perlin_with_random_vec noise;
    double scale;
};
