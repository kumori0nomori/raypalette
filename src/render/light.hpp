#pragma once

#include "math/color.hpp"
#include "math/polar.hpp"

#include <cstdint>

namespace raypalette {

struct PolarCoordinates {
  float radius = 3.0f;
  float theta_degrees = 45.0f;
  float phi_degrees = 0.0f;
};

enum class LightType : std::uint32_t {
  Point,
  RectArea,
  Directional,
};

struct PointLightParameters {
  Vec3 position{};
  Vec3 color{1.0f, 1.0f, 1.0f};
  // Relative radiant intensity; attenuated by inverse square distance.
  float radiant_intensity = 100.0f;
};

struct DirectionalLightParameters {
  Vec3 direction_to_light{0.0f, 1.0f, 0.0f};
  Vec3 color{1.0f, 1.0f, 1.0f};
  // Relative irradiance; no distance attenuation.
  float irradiance = 4.0f;
};

struct AreaLightParameters {
  Vec3 position{};
  Vec3 normal{0.0f, -1.0f, 0.0f};
  Vec3 color{1.0f, 1.0f, 1.0f};
  // Relative emitted radiance; used when area-light sampling is implemented.
  float radiance = 5.0f;
  float width = 1.0f;
  float height = 1.0f;
};

struct Light {
  LightType type = LightType::Point;
  PointLightParameters point;
  DirectionalLightParameters directional;
  AreaLightParameters area;
};

// Describes the lighting contribution from one light at a surface point.
struct LightSample {
  Vec3 direction_to_light;
  Vec3 radiance;
  float distance = 0.0f;
  // Probability density with respect to solid angle. Zero denotes a delta light.
  float pdf = 0.0f;
};

RAYPALETTE_HOST_DEVICE inline Light make_point_light(const PolarCoordinates& polar,
                                                     const Vec3& origin, const Vec3& color,
                                                     float radiant_intensity) {
  Light light;
  light.type = LightType::Point;
  light.point.position =
      origin + polar_to_cartesian(polar.radius, polar.theta_degrees, polar.phi_degrees);
  light.point.color = color;
  light.point.radiant_intensity = radiant_intensity;
  return light;
}

RAYPALETTE_HOST_DEVICE inline Light make_directional_light(float theta_degrees, float phi_degrees,
                                                           const Vec3& color, float irradiance) {
  Light light;
  light.type = LightType::Directional;
  light.directional.color = color;
  light.directional.irradiance = irradiance;
  light.directional.direction_to_light =
      normalized(polar_to_cartesian(1.0f, theta_degrees, phi_degrees));
  return light;
}

RAYPALETTE_HOST_DEVICE inline Light
make_rect_area_light(const PolarCoordinates& polar, const Vec3& origin, const Vec3& area_normal,
                     float width, float height, const Vec3& color, float radiance) {
  Light light;
  light.type = LightType::RectArea;
  light.area.position =
      origin + polar_to_cartesian(polar.radius, polar.theta_degrees, polar.phi_degrees);
  light.area.color = color;
  light.area.radiance = radiance;
  light.area.normal = normalized(area_normal);
  light.area.width = width;
  light.area.height = height;
  return light;
}

RAYPALETTE_HOST_DEVICE inline bool is_valid_light(const Light& light) {
  constexpr float minimum_direction_length_squared = 1.0e-12f;
  switch (light.type) {
  case LightType::Point:
    return is_unit_color(light.point.color) && is_finite(light.point.position) &&
           light.point.radiant_intensity >= 0.0f;
  case LightType::Directional:
    return is_unit_color(light.directional.color) &&
           is_finite(light.directional.direction_to_light) &&
           length_squared(light.directional.direction_to_light) >
               minimum_direction_length_squared &&
           light.directional.irradiance >= 0.0f;
  case LightType::RectArea:
    return is_unit_color(light.area.color) && is_finite(light.area.position) &&
           is_finite(light.area.normal) &&
           length_squared(light.area.normal) > minimum_direction_length_squared &&
           light.area.radiance >= 0.0f && light.area.width > 0.0f && light.area.height > 0.0f;
  }
  return false;
}

} // namespace raypalette
