#pragma once

#include "hit_record.h"
#include "interval.h"
#include "ray.h"

class hittable
{
  public:
    virtual ~hittable() = default;

    virtual bool hit(const ray &r, double ray_tmin, double ray_tmax,
                     hit_record &rec) const = 0;

    virtual bool hit(const ray &r, interval ray_t, hit_record &rec) const = 0;
};