#pragma once

#include <algorithm>

#include "interval.hpp"
#include "ray.hpp"
#include "vec3.hpp"

// NOLINTBEGIN
/*
光线与物体的交集是光线跟踪器的主要时间瓶颈，运行时间与物体的数量呈线性关系。
二分搜索的精神进行对数搜索。
两种最常见的排序方法是
    1）细分空间
    2）细分对象。

NOTE: 定义一个空间体积，如果光线和边界区间 没有交集，肯定和空间体积内部没有交集

NOTE: 分治，可以方便并行。一个大块边界体积，可以继续细分，允许细分的两个区间有交集
请注意，蓝色和红色的边界体积包含在紫色的边界体积中，但它们可能会重叠，而且它们没有排序——它们只是都在里面。
所以右边显示的树没有左右子级排序的概念；它们只是在里面。
https://raytracing.github.io/images/fig-2.01-bvol-hierarchy.jpg
*/
/*
NOTE:轴对齐边界矩形平行六面体（实际上，如果我们精确的话，这就是它们需要被称为的）轴对齐边界框或AABB
（在代码中，您还会遇到“边界框”的命名缩写“bbox”。）任何你想用来将光线与AABB相交的方法都可以。
我们只需要知道我们是否击中了它；我们不需要命中点或法线或任何我们需要显示对象的东西。

NOTE: 2D的交集: [颜色混合的部分体现交集]
https://raytracing.github.io/images/fig-2.02-2d-aabb.jpg

NOTE: 要确定射线是否击中一个区间，我们首先需要弄清楚射线是否击中边界
例如，在1D中，与两个平面相交的光线将产生光线参数t0，t1

NOTE: 将一维数学转化为2D或3D命中测试的关键观察是：如果一条射线与所有平面对包围的盒子相交
例如，在2D中，只有当光线与有界框相交时，绿色和蓝色才会重叠
https://raytracing.github.io/images/fig-2.04-ray-slab-interval.jpg
NOTE: 1. 上射线间隔不重叠，所以我们知道射线没有击中由绿色和蓝色平面包围的二维盒子。
NOTE: 2. 较低的射线间隔确实重叠，所以我们知道较低的射线确实击中了有界的盒子

NOTE: 2D 用两对平面 来取交集 得到 包围的二维盒子。 3D 应该是 3对：上下，左右，前后
NOTE: 用平面 来 表达 立方体的平面是合理的，立方体，6个面，3对面。
NOTE: 细看上面的例子。用平常的视角：与矩形边界是否有交点也是对的，延长成边界更容易计算吧？
*/
class aabb
{
  public:
    interval x, y, z; // NOTE: interval 确定了 3 对平面

    aabb() = default; // The default AABB is empty, since intervals are empty by default.

    constexpr aabb(const interval &x, const interval &y, const interval &z)
        : x(x), y(y), z(z)
    {
        pad_to_minimums();
    }

    constexpr aabb(const point3 &a, const point3 &b)
    {
        // 将两点a和b视为边界框的极值，因此我们不会 需要特定的最小/最大坐标顺序。
        x = (a[0] <= b[0]) ? interval(a[0], b[0]) : interval(b[0], a[0]);
        y = (a[1] <= b[1]) ? interval(a[1], b[1]) : interval(b[1], a[1]);
        z = (a[2] <= b[2]) ? interval(a[2], b[2]) : interval(b[2], a[2]);

        pad_to_minimums();
    }
    constexpr aabb(const aabb &box0, const aabb &box1)
    {
        x = interval(box0.x, box1.x);
        y = interval(box0.y, box1.y);
        z = interval(box0.z, box1.z);

        pad_to_minimums();
    }

    // NOTE: 轴
    [[nodiscard]] const interval &axis_interval(int n) const
    {
        if (n == 1)
            return y;
        if (n == 2)
            return z;
        return x;
    }

    /*
    光线与AABB相交：
    2D：
        interval_x ← compute_intersection_x (ray, x0, x1)
        interval_y ← compute_intersection_y (ray, y0, y1)
        return overlaps(interval_x, interval_y)

    3D：
        interval_x ← compute_intersection_x (ray, x0, x1)
        interval_y ← compute_intersection_y (ray, y0, y1)
        interval_z ← compute_intersection_z (ray, z0, z1)
        return overlaps(interval_x, interval_y, interval_z)
    */
    [[nodiscard]] bool hit(const ray &r, interval ray_t) const
    {
        const point3 &ray_orig = r.origin();
        const vec3 &ray_dir = r.direction();

        for (int axis = 0; axis < 3; axis++)
        {
            const interval &ax = axis_interval(axis);
            const double k_adinv = 1.0 / ray_dir[axis];

            auto t0 = (ax.min - ray_orig[axis]) * k_adinv;
            auto t1 = (ax.max - ray_orig[axis]) * k_adinv;

            if (t0 < t1)
            {
                // 正常方向：t0=进入，t1=离开
                ray_t.min = std::max(t0, ray_t.min);
                ray_t.max = std::min(t1, ray_t.max);
            }
            else
            {
                // 反向情况：t1=进入，t0=离开
                ray_t.min = std::max(t1, ray_t.min);
                ray_t.max = std::min(t0, ray_t.max);
            }

            /*
            数学上的精确含义
                ray_t.min = 射线最早能进入所有轴的时间
                ray_t.max = 射线最晚能离开所有轴的时间

            当 ray_t.max <= ray_t.min 时，意味着：
                进入时间 ≥ 离开时间 ← 这在物理上是不可能的！
                相交区间长度 ≤ 0 ← 没有有效的相交时间段

            为什么要这样判断？
            因为射线与AABB的相交必须是一个时间段，不能只是一个时间点：
                如果只是擦边而过（刚好在边界上相交），实际渲染中通常不算命中
                从数值精度考虑，浮点数计算可能有误差，这样判断更稳健
            */
            if (ray_t.max <= ray_t.min)
                return false; // NOTE: 任意一个区间，没有相交时间段
        }
        return true;
    }

    int longest_axis() const
    {
        // Returns the index of the longest axis of the bounding box.

        if (x.size() > y.size())
            return x.size() > z.size() ? 0 : 2;
        else
            return y.size() > z.size() ? 1 : 2;
    }

    static const aabb empty, universe;

  private:
    // 用极小的代价彻底解决了平坦物体在光线追踪中的数值稳定性问题
    // NOTE: 四边形：可能零厚度.可能平行于坐标轴！
    // NOTE: 球体：永远有厚度.半径>0
    // 四边形像纸片
    // 如果正好平行于桌面，从侧面看就是一条线
    // 很容易出现 "零厚度"视图

    void pad_to_minimums()
    {
        // 调整AABB，使其没有比某个三角形更窄的边，必要时进行填充。
        double delta = 0.0001;
        if (x.size() < delta)
            x = x.expand(delta);
        if (y.size() < delta)
            y = y.expand(delta);
        if (z.size() < delta)
            z = z.expand(delta);
    }
};
const aabb aabb::empty = aabb(interval::empty, interval::empty, interval::empty);
const aabb aabb::universe =
    aabb(interval::universe, interval::universe, interval::universe);

aabb operator+(const aabb &bbox, const vec3 &offset)
{
    return aabb(bbox.x + offset.x(), bbox.y + offset.y(), bbox.z + offset.z());
}

aabb operator+(const vec3 &offset, const aabb &bbox)
{
    return bbox + offset;
}

// NOLINTEND