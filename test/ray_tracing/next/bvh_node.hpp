#pragma once

#include "aabb.hpp"
#include "hittable.hpp"
#include "hittable_list.hpp"

// BVH也将是一个hittable——就像hittable列表一样。它实际上是一个容器，但它可以响应“这个射线击中你了吗？”的查询。
// 一个设计问题是，我们是否有两个类，一个用于树，一个用于树中的节点；
// 或者我们只有一个类，根只是我们指向的一个节点。
class bvh_node : public hittable // NOLINT
{
  public:
    explicit bvh_node(hittable_list list) : bvh_node(list.objects, 0, list.objects.size())
    {
        // There's a C++ subtlety here. This constructor (without span indices) creates an
        // implicit copy of the hittable list, which we will modify. The lifetime of the
        // copied list only extends until this constructor exits. That's OK, because we
        // only need to persist the resulting bounding volume hierarchy.
    }

    /*
任何效率结构，包括BVH，最复杂的部分是构建它。我们在构造函数中这样做。
BVH的一个很酷的事情是，只要bvh_node中的对象列表被分成两个子列表，命中函数就会起作用。
如果划分做得很好，那么两个孩子的边界框比他们的父边界框小，但这是为了速度而不是正确性
*/
    bvh_node(std::vector<std::shared_ptr<hittable>> &objects, size_t start, size_t end)
        : bbox_(aabb::empty)
    {
        // NOTE:我们可以加快BVH优化的速度。与其选择随机拆分轴，不如拆分包围框的最长轴以获得最大的细分
        // int axis = random_int(0, 2); // 随机选X、Y或Z轴
        for (size_t object_index = start; object_index < end; object_index++)
            bbox_ = aabb(bbox_,
                         objects[object_index]->bounding_box()); // NOTE: 合并成最小包围盒

        int axis = bbox_.longest_axis();

        auto comparator = (axis == 0)   ? box_x_compare
                          : (1 == axis) ? box_y_compare
                                        : box_z_compare;

        size_t object_span = end - start;

        if (object_span == 1)
        {
            // 只有一个物体：左右都指向它
            left_ = right_ = objects[start];
        }
        else if (object_span == 2)
        { // 只有两个物体：一个放左边，一个放右边
            left_ = objects[start];
            right_ = objects[start + 1];
        }
        else
        {
            // 多个物体：排序后递归分割
            std::sort(std::begin(objects) + start, std::begin(objects) + end, comparator);

            auto mid = start + (object_span / 2);
            left_ = make_shared<bvh_node>(objects, start, mid);
            right_ = make_shared<bvh_node>(objects, mid, end);
        }

        // 检查是否有边界框是为了防止你发送像无限平面这样没有边界框的东西。
        // bbox_ = aabb(left_->bounding_box(), right_->bounding_box());
    }

    /*
点击函数非常简单：检查节点的框是否被击中，如果是，检查子节点并整理任何细节。
*/
    bool hit(const ray &r, interval ray_t, hit_record &rec) const override
    {
        if (!bbox_.hit(r, ray_t))
            return false;

        bool hit_left = left_->hit(r, ray_t, rec);
        bool hit_right =
            right_->hit(r, interval(ray_t.min, hit_left ? rec.t : ray_t.max), rec);

        return hit_left || hit_right;
    }

    [[nodiscard]] aabb bounding_box() const override
    {
        return bbox_;
    }

  private:
    std::shared_ptr<hittable> left_;
    std::shared_ptr<hittable> right_;
    aabb bbox_;

    // NOLINTBEGIN
    static bool box_compare(const std::shared_ptr<hittable> &a,
                            const std::shared_ptr<hittable> &b, int axis_index)
    {
        auto a_axis_interval = a->bounding_box().axis_interval(axis_index);
        auto b_axis_interval = b->bounding_box().axis_interval(axis_index);
        return a_axis_interval.min < b_axis_interval.min;
    }

    static bool box_x_compare(const std::shared_ptr<hittable> &a,
                              const std::shared_ptr<hittable> &b)
    {
        return box_compare(a, b, 0);
    }

    static bool box_y_compare(const std::shared_ptr<hittable> &a,
                              const std::shared_ptr<hittable> &b)
    {
        return box_compare(a, b, 1);
    }

    static bool box_z_compare(const std::shared_ptr<hittable> &a,
                              const std::shared_ptr<hittable> &b)
    {
        return box_compare(a, b, 2);
    }
    // NOLINTEND
};