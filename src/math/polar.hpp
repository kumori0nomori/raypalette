#pragma once

#include "math/vec3.hpp"

namespace raypalette {

RAYPALETTE_HOST_DEVICE inline Vec3 polar_to_cartesian(float radius, float theta_degrees,
                                                      float phi_degrees) {
  constexpr float degrees_to_radians = 0.01745329251994329577f;
  const float theta = theta_degrees * degrees_to_radians;
  const float phi = phi_degrees * degrees_to_radians;
  const float sin_theta = sinf(theta);

  return {radius * sin_theta * cosf(phi), radius * cosf(theta), radius * sin_theta * sinf(phi)};
}

} // namespace raypalette
