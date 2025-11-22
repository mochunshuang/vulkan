#pragma once

#include "aabb.hpp"
#include "degrees_to_radians.hpp"
#include "hit_record.hpp"
#include "interval.hpp"
#include "ray.hpp"

struct hittable // NOLINT
{
    virtual ~hittable() = default;

    virtual bool hit(const ray &r, interval ray_t, hit_record &rec) const = 0;

    // 为Hittable构建边界框
    [[nodiscard]] virtual aabb bounding_box() const = 0; // NOLINT
};

/*
🎯 核心思想：移动光线，而不是移动物体
NOTE: 通过变换坐标系来模拟物体变换，而不是直接修改物体几何
    传统思维：移动物体: 物体位置 + 偏移量 = 新位置
    聪明方法：反向移动光线: 光线位置 - 偏移量 = 等效观察位置

法向量处理：
注意这里没有修改法向量，因为：
    平移不改变方向
    法向量是方向向量，平移不变

https://raytracing.github.io/images/fig-2.08-ray-box.jpg
*/
class translate : public hittable
{
  public:
    translate(std::shared_ptr<hittable> object, const vec3 &offset)
        : object(object), offset(offset),
          // NOTE: 包围盒也平移.需要 + 运算符
          bbox(object->bounding_box() + offset)
    {
    }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override
    {
        // 第1步：反向移动光线。几何意义：把观察者移到物体的"本地坐标系"
        ray offset_r(r.origin() - offset, r.direction(), r.time());

        // 第2步：在物体空间中检测相交，使用原始物体的相交检测逻辑
        if (!object->hit(offset_r, ray_t, rec))
            return false;

        // 第3步：将交点移回世界空间，将命中点从物体空间变换回世界空间
        rec.p += offset;

        return true;
    }
    aabb bounding_box() const override
    {
        return bbox;
    }

  private:
    std::shared_ptr<hittable> object;
    vec3 offset;
    aabb bbox;
};

/*
NOTE: 公式推导如下
步骤1：用极坐标表示原有点
x=rcosϕ
y=rsinϕ
步骤2：旋转后的新点
x′=rcos(ϕ+θ)
y′=rsin(ϕ+θ)
步骤3：使用三角恒等式展开,利用余弦和正弦的和角公式：
cos(ϕ+θ)=cosϕcosθ−sinϕsinθ
sin(ϕ+θ)=sinϕcosθ+cosϕsinθ
代入旋转后的新点：
x′=r(cosϕcosθ−sinϕsinθ)
y′=r(sinϕcosθ+cosϕsinθ)
步骤4：代回直角坐标,其中 x=rcosϕ,y=rsinϕ
x′=xcosθ−ysinθ
y′=ycosθ+xsinθ
NOTE:旋转一个物体不仅会改变交点，还会改变表面法向量，这将改变反射和折射的方向。
*/
class rotate_y : public hittable
{
  public:
    rotate_y(std::shared_ptr<hittable> object, double angle) : object(object)
    {
        // 将角度转换为弧度并预计算三角函数值
        auto radians = degrees_to_radians(angle);
        sin_theta = std::sin(radians);
        cos_theta = std::cos(radians);

        // 获取原始物体的轴对齐包围盒
        bbox = object->bounding_box();

        // 初始化最小和最大值，用于构建新的包围盒
        point3 min(infinity, infinity, infinity);
        point3 max(-infinity, -infinity, -infinity);

        // 遍历原始包围盒的8个角点（2×2×2）
        for (int i = 0; i < 2; i++) // x方向：min和max
        {
            for (int j = 0; j < 2; j++) // y方向：min和max
            {
                for (int k = 0; k < 2; k++) // z方向：min和max
                {
                    // 计算当前角点的坐标
                    // 当i=0取min，i=1取max，其他维度同理
                    auto x = (i * bbox.x.max) + ((1 - i) * bbox.x.min);
                    auto y = (j * bbox.y.max) + ((1 - j) * bbox.y.min);
                    auto z = (k * bbox.z.max) + ((1 - k) * bbox.z.min);

                    // 绕Y轴旋转当前角点
                    // 旋转矩阵：x' = x·cosθ + z·sinθ, z' = -x·sinθ + z·cosθ
                    auto newx = (cos_theta * x) + (sin_theta * z);
                    auto newz = (-sin_theta * x) + (cos_theta * z);

                    // 创建旋转后的测试点（Y坐标不变）
                    vec3 tester(newx, y, newz);

                    // 更新新包围盒的边界
                    for (int c = 0; c < 3; c++) // 遍历x,y,z三个分量
                    {
                        min[c] = std::fmin(min[c], tester[c]);
                        max[c] = std::fmax(max[c], tester[c]);
                    }
                }
            }
        }

        // 设置旋转后的新包围盒
        bbox = aabb(min, max);
    }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override
    {

        // Transform the ray from world space to object space.
        // 第1步：光线从世界空间到物体空间（反向旋转）
        // 使用逆旋转矩阵：x' = x·cosθ - z·sinθ, z' = x·sinθ + z·cosθ
        auto origin = point3((cos_theta * r.origin().x()) - (sin_theta * r.origin().z()),
                             r.origin().y(), // Y分量不变
                             (sin_theta * r.origin().x()) + (cos_theta * r.origin().z()));

        // 同样变换光线方向向量
        auto direction =
            vec3((cos_theta * r.direction().x()) - (sin_theta * r.direction().z()),
                 r.direction().y(), // Y分量不变
                 (sin_theta * r.direction().x()) + (cos_theta * r.direction().z()));

        // 创建在物体空间中的光线
        ray rotated_r(origin, direction, r.time());

        // Determine whether an intersection exists in object space (and if so, where).
        // 第2步：在物体空间中检测相交（物体保持轴对齐状态）
        if (!object->hit(rotated_r, ray_t, rec))
            return false;

        // Transform the intersection from object space back to world space.
        // 第3步：将相交结果从物体空间变换回世界空间（正向旋转）
        // 使用正旋转矩阵：x' = x·cosθ + z·sinθ, z' = -x·sinθ + z·cosθ
        rec.p = point3((cos_theta * rec.p.x()) + (sin_theta * rec.p.z()),
                       rec.p.y(), // Y坐标不变
                       (-sin_theta * rec.p.x()) + (cos_theta * rec.p.z()));

        // 同样变换法向量（对于纯旋转，法向量变换与点相同）
        rec.normal = vec3((cos_theta * rec.normal.x()) + (sin_theta * rec.normal.z()),
                          rec.normal.y(), // Y分量不变
                          (-sin_theta * rec.normal.x()) + (cos_theta * rec.normal.z()));

        return true;
    }
    aabb bounding_box() const override
    {
        return bbox;
    }

  private:
    std::shared_ptr<hittable> object; // 被旋转的物体
    double sin_theta;                 // 预计算的正弦值
    double cos_theta;                 // 预计算的余弦值
    aabb bbox;                        // 旋转后的轴对齐包围盒
};