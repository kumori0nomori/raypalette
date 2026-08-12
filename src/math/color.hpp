#pragma once

#include "math/vec3.hpp"

namespace raypalette {

RAYPALETTE_HOST_DEVICE constexpr float clamp_unit(float value) {
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

RAYPALETTE_HOST_DEVICE inline float srgb_to_linear_component(float value) {
  const float clamped = clamp_unit(value);
  return clamped <= 0.04045f 
         ? clamped / 12.92f
         : powf((clamped + 0.055f) / 1.055f, 2.4f);
}

RAYPALETTE_HOST_DEVICE inline float linear_to_srgb_component(float value) {
  const float clamped = clamp_unit(value);
  return clamped <= 0.0031308f 
         ? 12.92f * clamped
         : 1.055f * powf(clamped, 1.0f / 2.4f) - 0.055f;
}

RAYPALETTE_HOST_DEVICE inline Vec3 srgb_to_linear(const Vec3 &color) {
  return {srgb_to_linear_component(color.x),
          srgb_to_linear_component(color.y),
          srgb_to_linear_component(color.z)};
}

RAYPALETTE_HOST_DEVICE inline Vec3 linear_to_srgb(const Vec3 &color) {
  return {linear_to_srgb_component(color.x),
          linear_to_srgb_component(color.y),
          linear_to_srgb_component(color.z)};
}

RAYPALETTE_HOST_DEVICE inline Vec3 apply_exposure(const Vec3 &color,
                                                  float exposure_stops) {
  return color * exp2f(exposure_stops);
}

} // namespace raypalette
