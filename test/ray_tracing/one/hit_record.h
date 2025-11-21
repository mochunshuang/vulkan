#pragma once

#include <memory>
#include "ray.h"
#include "vec3.h"

class material;
class hit_record
{
  public:
    point3 p;    // 交点位置
    Vec3 normal; // 法向量
    double t;    // 光线参数
    // 后续可以添加：材质指针、纹理坐标等

    /*
hit_record只是一种将一堆参数塞进一个类中的方法，这样我们就可以将它们作为一个组发送。
当光线击中一个表面（例如一个特定的球体）时，hit_record中的材料指针将被设置为指向材料指针
*/
    std::shared_ptr<material> mat;

    bool front_face; // 新增：记录是正面还是背面
    void set_face_normal(const ray &r, const Vec3 &outward_normal)
    {
        // Sets the hit record normal vector.
        // NOTE: the parameter `outward_normal` is assumed to have unit length.

        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};