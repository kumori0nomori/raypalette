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

struct Light {
  LightType type = LightType::Point;
  Vec3 position;
  Vec3 color{1.0f, 1.0f, 1.0f};
  float intensity = 100.0f;
  Vec3 direction_to_light{0.0f, 1.0f, 0.0f};
  Vec3 area_normal{0.0f, -1.0f, 0.0f};
  float width = 1.0f;
  float height = 1.0f;
};

// Describes the lighting contribution from one light at a surface point.
struct LightSample {
  Vec3 direction_to_light;
  Vec3 radiance;
  float distance = 0.0f;
};

RAYPALETTE_HOST_DEVICE inline Light make_point_light(
    const PolarCoordinates &polar, const Vec3 &origin, const Vec3 &color,
    float intensity) {
  Light light;
  light.type = LightType::Point;
  light.position = origin + polar_to_cartesian(polar.radius, polar.theta_degrees,
                                               polar.phi_degrees);
  light.color = color;
  light.intensity = intensity;
  return light;
}

RAYPALETTE_HOST_DEVICE inline Light make_directional_light(
    float theta_degrees, float phi_degrees, const Vec3 &color, float intensity) {
  Light light;
  light.type = LightType::Directional;
  light.color = color;
  light.intensity = intensity;
  light.direction_to_light = normalized(
      polar_to_cartesian(1.0f, theta_degrees, phi_degrees));
  return light;
}

RAYPALETTE_HOST_DEVICE inline Light make_rect_area_light(
    const PolarCoordinates &polar, const Vec3 &origin, const Vec3 &area_normal,
    float width, float height, const Vec3 &color, float intensity) {
  Light light;
  light.type = LightType::RectArea;
  light.position = origin + polar_to_cartesian(polar.radius, polar.theta_degrees,
                                               polar.phi_degrees);
  light.color = color;
  light.intensity = intensity;
  light.area_normal = normalized(area_normal);
  light.width = width;
  light.height = height;
  return light;
}

RAYPALETTE_HOST_DEVICE inline bool is_valid_light(const Light &light) {
  constexpr float minimum_direction_length_squared = 1.0e-12f;
  if (!is_unit_color(light.color) || light.intensity < 0.0f) {
    return false;
  }

  switch (light.type) {
  case LightType::Point:
    return is_finite(light.position);
  case LightType::Directional:
    return is_finite(light.direction_to_light) &&
           length_squared(light.direction_to_light) > minimum_direction_length_squared;
  case LightType::RectArea:
    return is_finite(light.position) && is_finite(light.area_normal) &&
           length_squared(light.area_normal) > minimum_direction_length_squared &&
           light.width > 0.0f && light.height > 0.0f;
  }
  return false;
}

RAYPALETTE_HOST_DEVICE inline bool sample_light(const Light &light,
                                                const Vec3 &surface_position,
                                                LightSample &sample) {
  constexpr float minimum_distance_squared = 1.0e-12f;
  constexpr float infinite_distance = 1.0e30f;
  if (!is_valid_light(light)) {
    return false;
  }

  switch (light.type) {
  case LightType::Point: {
    const Vec3 offset = light.position - surface_position;
    const float distance_squared = length_squared(offset);
    if (distance_squared <= minimum_distance_squared) {
      return false;
    }
    const float inverse_distance = 1.0f / sqrtf(distance_squared);
    sample.direction_to_light = offset * inverse_distance;
    sample.distance = distance_squared * inverse_distance;
    sample.radiance = light.color * (light.intensity / distance_squared);
    return true;
  }
  case LightType::Directional:
    sample.direction_to_light = normalized(light.direction_to_light);
    sample.distance = infinite_distance;
    sample.radiance = light.color * light.intensity;
    return true;
  case LightType::RectArea:
    return false;
  }
  return false;
}

} // namespace raypalette
