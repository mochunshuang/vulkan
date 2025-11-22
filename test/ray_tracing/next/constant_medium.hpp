#pragma once

#include <algorithm>
#include <utility>

#include "hittable.hpp"
#include "material.hpp"
#include "texture.hpp"

/*
体积渲染流程：
边界检测：找到光线进出体积的位置

距离计算：确定光线在体积内的传播距离

随机采样：基于介质密度决定散射是否发生

散射处理：在散射点使用各向同性材质

应用场景：
🌫️ 雾效：低密度介质 + 灰白色调
💨 烟效：中等密度 + 动态纹理
🥛 浑浊液体：高密度 + 轻微颜色
🌅 大气散射：与距离相关的密度变化

🎯 凸 vs 非凸形状的对比
凸形状（正常工作）：
光线: →────┐         ┌────→
          │         │
      进入 ↓   体积内部   ↓ 离开
          │         │
          └─────────┘
球体/立方体：光线最多击中边界两次（进入+离开）

非凸形状（错误工作）：
光线: →────┐   ┌────┐   ┌────→
          │   │    │   │
      进入 ↓   │空洞│   ↓ 再进入
          │   │    │   │
          └───┘    └───┘
圆环体：光线可能击中边界四次（进入→离开→再进入→再离开）

✅ 总结
当前实现的核心限制：
    仅支持凸边界：球体、立方体、椭圆体等
    不支持复杂形状：圆环体、有孔物体、自相交形状
    简化假设：光线一旦离开体积就不会重新进入

这是一个性能与通用性的权衡——简化实现以换取计算效率，适用于大多数常见场景。
*/
class constant_medium : public hittable // NOLINT
{
  public:
    // 构造函数：使用纹理定义体积材质
    // 重要假设：边界形状必须是凸的（convex），因为算法假设光线一旦离开边界就不会重新进入
    // 适用：球体、立方体、圆锥体等凸形状
    // 不适用：圆环体、有孔的物体等非凸形状
    constant_medium(std::shared_ptr<hittable> boundary, double density,
                    const std::shared_ptr<texture> &tex)
        : boundary(std::move(boundary)),
          neg_inv_density(-1 / density), // 预计算负的密度倒数，用于指数衰减计算
          phase_function(make_shared<isotropic>(tex)) // 各向同性散射材质
    {
    }

    // 构造函数：使用固定颜色定义体积材质
    constant_medium(std::shared_ptr<hittable> boundary, double density,
                    const color &albedo)
        : boundary(std::move(boundary)), neg_inv_density(-1 / density),
          phase_function(std::make_shared<isotropic>(albedo)) // 各向同性散射材质
    {
    }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override
    {
        hit_record rec1; // 记录光线进入体积的位置
        hit_record rec2; // 记录光线离开体积的位置

        // 第1步：检测光线与体积边界的第一个交点（进入点）
        // 使用无限区间确保找到第一个交点
        // 假设：边界是凸的，所以第一个交点一定是进入点
        if (!boundary->hit(r, interval::universe, rec1))
            return false; // 光线没有击中体积边界

        // 第2步：检测光线与体积边界的第二个交点（离开点）
        // 从第一个交点稍后开始，避免自相交问题
        // 关键限制：这里假设光线一旦离开边界就不会再次进入
        // 对于凸形状这是成立的，但对于有孔的非凸形状（如圆环）会出错
        // 因为光线可能会：进入→离开→再进入→再离开
        if (!boundary->hit(r, interval(rec1.t + 0.0001, infinity), rec2))
            return false; // 光线没有完全穿过体积（可能切线或内部光源）

        // 第3步：将交点时间限制在有效的光线时间范围内
        rec1.t = std::max(rec1.t, ray_t.min); // 确保进入点不小于光线最小时间
        rec2.t = std::min(rec2.t, ray_t.max); // 确保离开点不大于光线最大时间

        // 检查时间区间有效性
        // 注意：对于非凸形状，rec1和rec2可能不是真正的"进入-离开"对
        // 可能是"进入-再进入"或"离开-再离开"的错误配对
        if (rec1.t >= rec2.t)
            return false; // 无效的时间区间，没有实际穿透体积

        // 确保进入时间不为负（光线起点在体积外的情况）
        rec1.t = std::max<double>(rec1.t, 0);

        // 第4步：计算光线在体积内部传播的实际距离
        // 这里隐含假设：从rec1到rec2之间的所有点都在体积内部
        // 对于凸形状成立，对于有孔的形状不成立
        auto ray_length = r.direction().length(); // 光线方向向量的长度
        auto distance_inside_boundary = (rec2.t - rec1.t) * ray_length; // 体积内传播距离

        // 第5步：基于指数衰减的随机散射距离采样
        // 使用公式：hit_distance = -ln(rand) / density
        // 这模拟了光线在介质中传播时的指数衰减特性
        auto hit_distance = neg_inv_density * std::log(random_double());

        // 第6步：判断散射是否发生在体积内部
        if (hit_distance > distance_inside_boundary)
            return false; // 散射距离超过体积厚度，光线直接穿透

        // 第7步：计算实际的散射点位置
        rec.t = rec1.t + (hit_distance / ray_length); // 将距离转换回时间参数
        rec.p = r.at(rec.t);                          // 计算散射点的3D坐标

        // 第8步：设置命中记录（体积散射的特殊处理）
        rec.normal = vec3(1, 0, 0); // 任意法向量，体积散射没有表面概念
        rec.front_face = true;      // 任意朝向，体积内部没有内外之分

        rec.mat = phase_function; // 设置各向同性散射材质

        return true; // 成功在体积内部发生散射
    }

    aabb bounding_box() const override
    {
        // 体积的包围盒与其边界物体相同
        return boundary->bounding_box();
    }

  private:
    std::shared_ptr<hittable> boundary;       // 定义体积形状的边界物体（如球体、立方体）
    double neg_inv_density;                   // 负的密度倒数，预计算用于性能优化
    std::shared_ptr<material> phase_function; // 相位函数，定义散射的方向分布（各向同性）
};
