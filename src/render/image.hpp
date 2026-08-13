#pragma once

#include "math/vec3.hpp"

#include <cstddef>
#include <vector>

namespace raypalette {

struct Image {
  int width = 0;
  int height = 0;
  std::vector<Vec3> pixels;

  const Vec3 &at(int x, int y) const {
    return pixels[static_cast<std::size_t>(y) * width + x];
  }
};

} // namespace raypalette
