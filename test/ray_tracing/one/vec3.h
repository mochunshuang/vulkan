#pragma once
#include <cmath>
#include "random_double.h"

// NOLINTBEGIN
class Vec3
{
  public:
    double e[3]; // NOTE:可以使用单精度

    constexpr Vec3() : e{0, 0, 0} {}
    constexpr Vec3(double e0, double e1, double e2) : e{e0, e1, e2} {}

    constexpr double x() const
    {
        return e[0];
    }
    constexpr double y() const
    {
        return e[1];
    }
    constexpr double z() const
    {
        return e[2];
    }

    constexpr Vec3 operator-() const
    {
        return Vec3(-e[0], -e[1], -e[2]);
    }
    constexpr double operator[](int i) const
    {
        return e[i];
    }
    constexpr double &operator[](int i)
    {
        return e[i];
    }

    constexpr Vec3 &operator+=(const Vec3 &v)
    {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }

    constexpr Vec3 &operator*=(double t)
    {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    constexpr Vec3 &operator/=(double t)
    {
        return *this *= 1 / t;
    }

    constexpr double length() const
    {
        return std::sqrt(length_squared());
    }

    constexpr double length_squared() const
    {
        return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
    }

    constexpr bool near_zero() const
    {
        // Return true if the vector is close to zero in all dimensions.
        auto s = 1e-8;
        return (std::fabs(e[0]) < s) && (std::fabs(e[1]) < s) && (std::fabs(e[2]) < s);
    }

    /**
 NOTE: 产生随机向量。 漫反射是光线反射
     */
    constexpr static Vec3 random()
    {
        return Vec3(random_double(), random_double(), random_double());
    }
    constexpr static Vec3 random(double min, double max)
    {
        return Vec3(random_double(min, max), random_double(min, max),
                    random_double(min, max));
    }
};

// point3 is just an alias for vec3, but useful for geometric clarity in the code.
using point3 = Vec3;

// Vector Utility Functions

constexpr Vec3 operator+(const Vec3 &u, const Vec3 &v)
{
    return Vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

constexpr Vec3 operator-(const Vec3 &u, const Vec3 &v)
{
    return Vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

constexpr Vec3 operator*(const Vec3 &u, const Vec3 &v)
{
    return Vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

constexpr Vec3 operator*(double t, const Vec3 &v)
{
    return Vec3(t * v.e[0], t * v.e[1], t * v.e[2]);
}

constexpr Vec3 operator*(const Vec3 &v, double t)
{
    return t * v;
}

constexpr Vec3 operator/(const Vec3 &v, double t)
{
    return (1 / t) * v;
}

constexpr double dot(const Vec3 &u, const Vec3 &v)
{
    return u.e[0] * v.e[0] + u.e[1] * v.e[1] + u.e[2] * v.e[2];
}

constexpr Vec3 cross(const Vec3 &u, const Vec3 &v)
{
    return Vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1], u.e[2] * v.e[0] - u.e[0] * v.e[2],
                u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

constexpr Vec3 unit_vector(const Vec3 &v)
{
    return v / v.length();
}

constexpr Vec3 random_in_unit_disk()
{
    while (true)
    {
        auto p = Vec3(random_double(-1, 1), random_double(-1, 1), 0);
        if (p.length_squared() < 1)
            return p;
    }
}

constexpr Vec3 random_unit_vector()
{
    /*
拒绝采样（Rejection Sampling）算法，用于在单位球面上生成均匀分布的随机点
// 优点：均匀分布，实现简单
// 缺点：可能拒绝48%的样本

拒绝区域：
单位球外的点：x² + y² + z² > 1
太靠近原点的点：x² + y² + z² < 1e-160
*/
    while (true)
    {
        auto p = Vec3::random(-1, 1);    // 每个分量在[-1,1]范围内
        auto lensq = p.length_squared(); // x² + y² + z²
        // 拒绝位于中心周围“黑洞”内的点。通过双精度（64位浮点数），我们可以安全地支持大于10-160的值
        if (1e-160 < lensq && lensq <= 1.0)
            return p / sqrt(lensq); // 投影到单位球面
    }
}
/*
基于法向量的半球均匀采样，是漫反射和全局光照的核心
这个简洁的函数实现了：
    物理正确的漫反射采样
    半球上的均匀分布
    高效的实现（50%直接接受，50%简单翻转）
    数值稳定（避免奇点问题）

是蒙特卡洛光线追踪中漫反射采样的标准方法！

为什么这样是物理正确的漫反射？
能量守恒：光线只在表面外部散射
余弦定律：自然满足 Lambert's Cosine Law
均匀分布：在半球上均匀采样
*/
constexpr Vec3 random_on_hemisphere(const Vec3 &normal)
{
    // 步骤1：生成单位球面上的随机点
    Vec3 on_unit_sphere = random_unit_vector();
    // 步骤2：检查点是否在目标半球
    if (dot(on_unit_sphere, normal) > 0.0) // In the same hemisphere as the normal
        return on_unit_sphere;
    else
        return -on_unit_sphere; // 翻转到正确半球
}

/*
反射光线方向为v+2b
用v的投影长度来缩放法向量到n 旧得到 b

             反弹的入射向量 v
           ↗
     n    /|
      ^  / | b
      | /  |
    \ |/   |
      ●━━━━┿━━━━━● 表面
       \   |
        \  |
         \ |
          \|
           ● 反射向量 r

https://raytracing.github.io/images/fig-1.15-reflection.jpg
*/
constexpr Vec3 reflect(const Vec3 &v, const Vec3 &n)
{
    return v - 2 * dot(v, n) * n;
}

/*
透明材料，如水、玻璃和钻石，都属于电介质。当光线照射到它们时，会分裂成反射光和折射（透射）光。我们将通过随机选择反射或折射来处理这种情况，每次相互作用只生成一条散射光线。

先快速回顾一下相关术语：

    反射光：光线击中表面后以新方向"弹射"出去。

    折射光：当光线从一种介质进入另一种介质时会发生弯曲（比如从空气进入玻璃或水）。这就是为什么部分插入水中的铅笔看起来是弯曲的。

折射光的弯曲程度由材料的折射率决定。通常，这个单一数值描述了光线从真空进入材料时的弯曲程度。玻璃的折射率约为1.5-1.7，钻石约为2.4，空气的折射率很小，为1.000293。
当一种透明材料嵌入另一种透明材料中时，可以用相对折射率来描述折射：即物体材料的折射率除以外围材料的折射率。例如，如果要渲染水下的玻璃球，那么玻璃球的有效折射率为1.125。这是由玻璃的折射率（1.5）除以水的折射率（1.333）得出的。

通过快速网络搜索，您可以找到大多数常见材料的折射率。
*/
constexpr Vec3 refract(const Vec3 &uv, const Vec3 &n, double etai_over_etat)
{
    auto cos_theta = std::fmin(dot(-uv, n), 1.0);
    Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    Vec3 r_out_parallel = -std::sqrt(std::fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}
// NOLINTEND