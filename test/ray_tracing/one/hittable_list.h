#pragma once

#include "hittable.h"

#include <memory>
#include <utility>
#include <vector>

class hittable_list : public hittable
{

  public:
    std::vector<std::shared_ptr<hittable>> objects;

    hittable_list() = default;
    explicit hittable_list(std::shared_ptr<hittable> object)
    {
        add(std::move(object));
    }

    void clear()
    {
        objects.clear();
    }

    void add(std::shared_ptr<hittable> object)
    {
        objects.push_back(object);
    }

    // NOTE: 遍历。但是不会覆盖
    /*
光线方向 →
0.001 ━━━●━━━━━━━━━━━━━━ 1000.0
        C(t=1.0)  B(t=3.0)  A(t=5.0)

检测过程：
1. 检测C：找到交点t=1.0，更新最近距离为1.0
2. 检测B：只在[0.001, 1.0]范围内检测，B在3.0处，检测不到
3. 检测A：只在[0.001, 1.0]范围内检测，A在5.0处，检测不到
*/
    bool hit(const ray &r, double ray_tmin, double ray_tmax,
             hit_record &rec) const override
    {
        hit_record temp_rec;
        bool hit_anything = false;

        /*
NOTE: 这个实现是正确的，因为：
    closest_so_far 动态更新：始终保存当前最近的交点距离
    检测范围逐步缩小：后续物体只在比当前最近点更近的范围内检测
    rec 只记录最近交点：每次更新都是找到更近的交点
所以 rec 不会被错误覆盖，而是正确地更新为最近的交点！
*/
        auto closest_so_far = ray_tmax;

        for (const auto &object : objects)
        {
            if (object->hit(r, ray_tmin, closest_so_far, temp_rec))
            {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }

    bool hit(const ray &r, interval ray_t, hit_record &rec) const override
    {
        hit_record temp_rec;
        bool hit_anything = false;
        auto closest_so_far = ray_t.max;

        for (const auto &object : objects)
        {
            if (object->hit(r, interval(ray_t.min, closest_so_far), temp_rec))
            {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }
};