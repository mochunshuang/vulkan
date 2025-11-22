#pragma once

#include "hittable.hpp"

#include <memory>
#include <vector>

// 带边界框的hittable列表
struct hittable_list : public hittable
{

  public:
    std::vector<std::shared_ptr<hittable>> objects; // NOLINT

    hittable_list() = default;
    explicit hittable_list(const std::shared_ptr<hittable> &object)
    {
        add(object);
    }

    void clear()
    {
        objects.clear();
    }

    void add(const std::shared_ptr<hittable> &object)
    {
        objects.push_back(object);

        bbox_ = aabb(bbox_, object->bounding_box());
    }

    // NOTE: 遍历。但是不会覆盖
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

    [[nodiscard]] aabb bounding_box() const override
    {
        return bbox_;
    }

  private:
    aabb bbox_; // NOTE: 添加 AABB矩形
};