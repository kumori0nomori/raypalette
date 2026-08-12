#pragma once

#include "math/color.hpp"
#include "math/polar.hpp"

namespace raypalette {

struct PolarCoordinates {
  float radius = 3.0f;
  float theta_degrees = 45.0f;
  float phi_degrees = 0.0f;
};

struct PointLight {
  Vec3 position;
  Vec3 color{1.0f, 1.0f, 1.0f};
  float intensity = 100.0f;
};

RAYPALETTE_HOST_DEVICE inline PointLight point_light_from_polar(
    const PolarCoordinates &polar, const Vec3 &origin, const Vec3 &color,
    float intensity) {
  return {origin + polar_to_cartesian(polar.radius, polar.theta_degrees,
                                      polar.phi_degrees),
          color, intensity};
}

RAYPALETTE_HOST_DEVICE inline bool is_valid_point_light(const PointLight &light) {
  return is_finite(light.position) && is_unit_color(light.color) &&
         light.intensity >= 0.0f;
}

} // namespace raypalette
