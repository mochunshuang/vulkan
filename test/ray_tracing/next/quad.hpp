#pragma once

#include <utility>

#include "hittable.hpp"
#include "hittable_list.hpp"

/*
平面：
NOTE: 代数形式：
1. 几何定义
一个平面可以由：
    平面上一个点：P0 = (x0,y0,z0)
    平面的法向量: n = (A,B,C) //A, B, C 分别表示在X、Y、Z方向上的"分量",3D的法向量
2. 向量推导
    对于平面上任意点：P1 = (x,y,z)
    向量 P0P1 = (x-x0,y-y0,z-z0),必须与法向量n垂直
3. 两个垂直向量的点积为0：
    n·P0P1 = 0
    A(x-x₀) + B(y-y₀) + C(z-z₀) = 0
    Ax - Ax₀ + By - By₀ + Cz - Cz₀ = 0
    Ax + By + Cz + (-Ax₀ - By₀ - Cz₀) = 0
    令：D0 = -Ax₀ - By₀ - Cz₀
        D = -D0
    得：Ax + By + Cz + D0 = 0
        Ax + By + Cz = D
NOTE: 向量形式："平面是所有在法向量方向上具有相同投影值的点的集合"
     n⋅v=D

NOTE: 射线 R(t)=P+td

NOTE: 得到公式：n·(P+td) = D => t = (D-n·P)/(n·d)
NOTE: 这给了我们t，我们可以将其插入射线方程以找到交叉点
      如果射线平行于平面，则为零。
      参数小于最小可接受值，我们也记录未命中
*/

/*

     v
    ↗
   /
  /  高 = |v|·sinθ
 /
u ──────────>
平行四边形中:
NOTE: 面积 = 底 × 高 = |u| × (|v|*sinθ)

🎯 点积 (Dot Product) - 投影仪
物理意义：一个向量在另一个向量上的投影长度
点积 = |A| × |B| × cosθ
💡 在物理中：
功 = 力 · 位移 · cosθ （力在位移方向的分量）
功率 = 力 · 速度 · cosθ

🎯 叉积 (Cross Product) - 旋转器
物理意义：产生垂直于两个向量的旋转效应
叉积大小 = |A| × |B| × sinθ
叉积方向 = 右手定则方向
🎪 比喻：拧螺丝
扳手力 = 向量A
扳手长度 = 向量B
旋转效果 = 叉积大小
旋转轴 = 叉积方向
💡 在物理中：
力矩 = 力 × 力臂 （产生旋转）
角动量 = 位置 × 动量
洛伦兹力 = 电荷 × 速度 × 磁场

💡 终极理解：
点积是"对齐度测量"，叉积是"垂直度测量"
你想知道两个方向多一致？用点积
你想知道两个方向多垂直？用叉积
你想知道旋转效果多大？用叉积大小

*/
/*
NOTE: 射线平面交叉点
https://raytracing.github.io/images/fig-2.06-ray-plane.jpg
NOTE: 如果交叉于点 P，如果在平行四边形内可以使用u,v 和 Q 表示
NOTE: 比如：Q+(1)u+(0.5)v，即 uv 平面内 P=(1,0.5)
NOTE: 直接魔法建立连接：平面坐标 α 和 β：
NOTE:    P = Q + αu + βv
令：p = P - Q,
则： p = αu + βv
为了求解α，我们可以利用向量叉积的性质：叉积的结果与两个向量都垂直，从而消去另一个参数。
首先，将p与v叉乘：
p × v = (α u + β v) × v = α (u × v) + β (v × v) = α n + 0 = α n
然后，我们点积n：
(p × v) · n = α (n · n)
所以，α = (p × v) · n / (n · n)

同理，为了求解β，将u与p叉乘：
u × p = u × (α u + β v) = α (u × u) + β (u × v) = 0 + β n = β n
然后点积n：
(u × p) · n = β (n · n)
所以，β = (u × p) · n / (n · n)

注意，n · n = |n|^2，所以分母是标量。
我们预计算一个向量 w = n / (n · n)，这样：
α = (p × v) · w
β = (u × p) · w

*/
class quad : public hittable
{
  public:
    quad(const point3 &Q, const vec3 &u, const vec3 &v, std::shared_ptr<material> mat)
        : Q(Q), u(u), v(v), mat(std::move(mat)),
          // cross(u, v): 获取原始法向量
          // 几何意义：通过四边形的两个边向量,得到垂直于四边形平面的法向量
          // NOTE: 叉乘得到法向量
          normal{unit_vector(cross(u, v))},
          // NOTE:几何意义：计算参考点Q在法向量方向上的投影长度，这就是平面方程中的常数D
          D{dot(normal, Q)}, // NOTE: D是常数，对平面内任意点都得成立
          w{cross(u, v) / dot(cross(u, v), cross(u, v))}
    {
        set_bounding_box();
    }

    virtual void set_bounding_box()
    {
        // Compute the bounding box of all four vertices.
        auto bbox_diagonal1 = aabb(Q, Q + u + v);
        auto bbox_diagonal2 = aabb(Q + u, Q + v);
        bbox = aabb(bbox_diagonal1, bbox_diagonal2);
    }

    [[nodiscard]] aabb bounding_box() const override
    {
        return bbox;
    }

    /*
光线-四边形相交判定将分三步进行：
    找到包含该四边形的平面，
    求解光线与该包含四边形的平面的交点，
    确定命中点是否位于四边形内部。
*/
    bool hit(const ray &r, interval ray_t, hit_record &rec) const override
    {
        // 第1步：检查光线是否平行于平面
        auto denom = dot(normal, r.direction());
        // 如果光线方向与法向量垂直（点积≈0），说明光线平行于平面
        // 使用 1e-8 作为容差，避免浮点数精度问题
        if (std::fabs(denom) < 1e-8)
            return false;

        // 第2步：计算交点参数t
        // NOTE: t = (D-n·P)/(n·d)
        // 如果命中点参数t在射线间隔之外，则返回false
        auto t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t))
            return false;

        // 第3步：计算交点坐标并设置命中记录
        auto intersection = r.at(t);

        vec3 planar_hitpt_vector = intersection - Q;        // p = P - Q
        auto alpha = dot(w, cross(planar_hitpt_vector, v)); // α坐标
        auto beta = dot(w, cross(u, planar_hitpt_vector));  // β坐标

        // 第4步：内部点检测
        if (!is_interior(alpha, beta, rec))
            return false;

        rec.t = t;
        rec.p = intersection;
        rec.mat = mat;
        rec.set_face_normal(r, normal);

        return true;
    }

    /*
     β
     ↑
    1.0┌─────────┐
      │         │
      │    ● P  │ ← 交点P的坐标(α,β)
      │         │
    0.0└─────────┘→ α
       0.0      1.0
    */
    virtual bool is_interior(double a, double b, hit_record &rec) const
    {
        interval unit_interval = interval(0, 1);

        // 检查α,β是否在[0,1]范围内 //NOTE: 超过1肯定越界
        // 原理: https://raytracing.github.io/images/fig-2.07-quad-coords.jpg
        if (!unit_interval.contains(a) || !unit_interval.contains(b))
            return false;

        // 设置纹理坐标
        rec.u = a;
        rec.v = b;
        return true;
    }

  private:
    /*
原理图：
https://raytracing.github.io/images/fig-2.05-quad-def.jpg
我们将使用三个几何实体来定义一个四边形：
    Q：起始角点。
    u：代表第一条边的向量。Q + u 得到与 Q 相邻的一个角点。
    v：代表第二条边的向量。Q + v 得到与 Q 相邻的另一个角点。

与 Q 相对的四边形角点由 Q + u + v
给出。这些值都是三维的，即使四边形本身是二维对象。例如，一个角点在原点、沿 Z 方向延伸 2
个单位、沿 Y 方向延伸 1 个单位的四边形，其值应为：Q = (0,0,0), u = (0,0,2), v = (0,1,0)。

NOTE: 3维 表示 2维. 四边形参数
四边形是平坦的，因此如果它位于XY、YZ 或ZX平面内，其轴对齐包围盒在某一维度上的厚度将为零。
这可能导致光线相交的数值问题。
NOTE:为了修正这种情况，我们插入一个小的填充量，以确保新构建的 AABB 总是具有非零体积
*/
    point3 Q;
    vec3 u, v;

    std::shared_ptr<material> mat;
    aabb bbox;

    // NOTE: 平面方程参数
    vec3 normal;
    double D;

    vec3 w; // NOTE: 四边形常量向量
};

/*
康奈尔盒子通常有两个块。它们相对于墙壁旋转。
首先，让我们创建一个返回一个盒子的函数，通过创建一个由六个矩形组成的hittable_list
*/
inline std::shared_ptr<hittable_list> box(const point3 &a, const point3 &b,
                                          std::shared_ptr<material> mat)
{
    // Returns the 3D box (six sides) that contains the two opposite vertices a & b.

    auto sides = std::make_shared<hittable_list>();

    // Construct the two opposite vertices with the minimum and maximum coordinates.
    auto min =
        point3(std::fmin(a.x(), b.x()), std::fmin(a.y(), b.y()), std::fmin(a.z(), b.z()));
    auto max =
        point3(std::fmax(a.x(), b.x()), std::fmax(a.y(), b.y()), std::fmax(a.z(), b.z()));

    auto dx = vec3(max.x() - min.x(), 0, 0);
    auto dy = vec3(0, max.y() - min.y(), 0);
    auto dz = vec3(0, 0, max.z() - min.z());

    sides->add(
        make_shared<quad>(point3(min.x(), min.y(), max.z()), dx, dy, mat)); // front
    sides->add(
        make_shared<quad>(point3(max.x(), min.y(), max.z()), -dz, dy, mat)); // right
    sides->add(
        make_shared<quad>(point3(max.x(), min.y(), min.z()), -dx, dy, mat));       // back
    sides->add(make_shared<quad>(point3(min.x(), min.y(), min.z()), dz, dy, mat)); // left
    sides->add(make_shared<quad>(point3(min.x(), max.y(), max.z()), dx, -dz, mat)); // top
    sides->add(
        make_shared<quad>(point3(min.x(), min.y(), min.z()), dx, dz, mat)); // bottom

    return sides;
}