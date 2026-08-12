#pragma once

#include "math/vec3.hpp"

namespace raypalette {

struct Ray {
  Vec3 origin;
  Vec3 direction;

  RAYPALETTE_HOST_DEVICE constexpr Vec3 at(float distance) const {
    return origin + distance * direction;
  }
};

} // namespace raypalette
