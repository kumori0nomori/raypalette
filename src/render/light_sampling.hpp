#pragma once

#include "render/light.hpp"

namespace raypalette {

RAYPALETTE_HOST_DEVICE inline bool sample_light(const Light& light, const Vec3& surface_position,
                                                LightSample& sample) {
  constexpr float minimum_distance_squared = 1.0e-12f;
  constexpr float infinite_distance = 1.0e30f;
  if (!is_valid_light(light)) {
    return false;
  }

  switch (light.type) {
  case LightType::Point: {
    const Vec3 offset = light.point.position - surface_position;
    const float distance_squared = length_squared(offset);
    if (distance_squared <= minimum_distance_squared) {
      return false;
    }
    const float inverse_distance = 1.0f / sqrtf(distance_squared);
    sample.direction_to_light = offset * inverse_distance;
    sample.distance = distance_squared * inverse_distance;
    sample.radiance = light.point.color * (light.point.radiant_intensity / distance_squared);
    sample.pdf = 0.0f;
    return true;
  }
  case LightType::Directional:
    sample.direction_to_light = normalized(light.directional.direction_to_light);
    sample.distance = infinite_distance;
    sample.radiance = light.directional.color * light.directional.irradiance;
    sample.pdf = 0.0f;
    return true;
  case LightType::RectArea:
    return false;
  }
  return false;
}

RAYPALETTE_HOST_DEVICE inline bool sample_area_light(const Light& light,
                                                     const Vec3& surface_position, float sample_u,
                                                     float sample_v, LightSample& sample) {
  constexpr float minimum_distance_squared = 1.0e-12f;
  if (!is_valid_light(light) || light.type != LightType::RectArea || sample_u < -0.5f ||
      sample_u > 0.5f || sample_v < -0.5f || sample_v > 0.5f) {
    return false;
  }

  const Vec3 normal = normalized(light.area.normal);
  const Vec3 reference_axis =
      fabsf(normal.y) < 0.999f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
  const Vec3 tangent = normalized(cross(reference_axis, normal));
  const Vec3 bitangent = cross(normal, tangent);
  const Vec3 light_position = light.area.position + tangent * (sample_u * light.area.width) +
                              bitangent * (sample_v * light.area.height);
  const Vec3 offset = light_position - surface_position;
  const float distance_squared = length_squared(offset);
  if (distance_squared <= minimum_distance_squared) {
    return false;
  }

  const float inverse_distance = 1.0f / sqrtf(distance_squared);
  sample.direction_to_light = offset * inverse_distance;
  sample.distance = distance_squared * inverse_distance;
  const float light_cosine = fmaxf(0.0f, dot(normal, -sample.direction_to_light));
  const float area = light.area.width * light.area.height;
  if (light_cosine <= 0.0f || area <= 0.0f) {
    return false;
  }
  sample.radiance =
      light.area.color * (light.area.radiance * area * light_cosine / distance_squared);
  sample.pdf = distance_squared / (light_cosine * area);
  return true;
}

} // namespace raypalette
