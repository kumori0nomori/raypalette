#pragma once

#include "math/vec3.hpp"

namespace raypalette {

struct Hsv {
  float hue = 0.0f;
  float saturation = 0.0f;
  float value = 0.0f;
};

RAYPALETTE_HOST_DEVICE constexpr float clamp_unit(float value) {
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

RAYPALETTE_HOST_DEVICE inline bool is_unit_color(const Vec3& color) {
  return is_finite(color) && color.x >= 0.0f && color.x <= 1.0f && color.y >= 0.0f &&
         color.y <= 1.0f && color.z >= 0.0f && color.z <= 1.0f;
}

RAYPALETTE_HOST_DEVICE inline bool is_nonnegative_color(const Vec3& color) {
  return is_finite(color) && color.x >= 0.0f && color.y >= 0.0f && color.z >= 0.0f;
}

RAYPALETTE_HOST_DEVICE inline float srgb_to_linear_component(float value) {
  const float clamped = clamp_unit(value);
  return clamped <= 0.04045f ? clamped / 12.92f : powf((clamped + 0.055f) / 1.055f, 2.4f);
}

RAYPALETTE_HOST_DEVICE inline float linear_to_srgb_component(float value) {
  const float clamped = clamp_unit(value);
  return clamped <= 0.0031308f ? 12.92f * clamped : 1.055f * powf(clamped, 1.0f / 2.4f) - 0.055f;
}

RAYPALETTE_HOST_DEVICE inline Vec3 srgb_to_linear(const Vec3& color) {
  return {srgb_to_linear_component(color.x), srgb_to_linear_component(color.y),
          srgb_to_linear_component(color.z)};
}

RAYPALETTE_HOST_DEVICE inline Vec3 linear_to_srgb(const Vec3& color) {
  return {linear_to_srgb_component(color.x), linear_to_srgb_component(color.y),
          linear_to_srgb_component(color.z)};
}

RAYPALETTE_HOST_DEVICE inline Hsv srgb_to_hsv(const Vec3& color) {
  const float red = clamp_unit(color.x);
  const float green = clamp_unit(color.y);
  const float blue = clamp_unit(color.z);
  const float maximum = fmaxf(red, fmaxf(green, blue));
  const float minimum = fminf(red, fminf(green, blue));
  const float chroma = maximum - minimum;
  if (chroma <= 1.0e-6f) {
    return {0.0f, 0.0f, maximum};
  }

  float hue = 0.0f;
  if (maximum == red) {
    hue = (green - blue) / chroma;
  } else if (maximum == green) {
    hue = 2.0f + (blue - red) / chroma;
  } else {
    hue = 4.0f + (red - green) / chroma;
  }
  hue *= 1.0f / 6.0f;
  if (hue < 0.0f) {
    hue += 1.0f;
  }
  return {hue, chroma / maximum, maximum};
}

RAYPALETTE_HOST_DEVICE inline Vec3 hsv_to_srgb(const Hsv& color) {
  const float hue = color.hue - floorf(color.hue);
  const float saturation = clamp_unit(color.saturation);
  const float value = clamp_unit(color.value);
  const float chroma = value * saturation;
  const float hue_sector = hue * 6.0f;
  const float secondary = chroma * (1.0f - fabsf(fmodf(hue_sector, 2.0f) - 1.0f));
  const float minimum = value - chroma;
  if (hue_sector < 1.0f) {
    return {chroma + minimum, secondary + minimum, minimum};
  }
  if (hue_sector < 2.0f) {
    return {secondary + minimum, chroma + minimum, minimum};
  }
  if (hue_sector < 3.0f) {
    return {minimum, chroma + minimum, secondary + minimum};
  }
  if (hue_sector < 4.0f) {
    return {minimum, secondary + minimum, chroma + minimum};
  }
  if (hue_sector < 5.0f) {
    return {secondary + minimum, minimum, chroma + minimum};
  }
  return {chroma + minimum, minimum, secondary + minimum};
}

RAYPALETTE_HOST_DEVICE inline Vec3 hsv_cylinder_position(const Hsv& color) {
  constexpr float kTwoPi = 6.28318530717958647692f;
  const float angle = kTwoPi * color.hue;
  return {color.saturation * cosf(angle), color.value, color.saturation * sinf(angle)};
}

RAYPALETTE_HOST_DEVICE inline Vec3 apply_exposure(const Vec3& color, float exposure_stops) {
  return color * exp2f(exposure_stops);
}

RAYPALETTE_HOST_DEVICE inline Vec3 reinhard_tonemap(const Vec3& color) {
  return {color.x / (1.0f + color.x), color.y / (1.0f + color.y), color.z / (1.0f + color.z)};
}

RAYPALETTE_HOST_DEVICE inline Vec3 prepare_for_display(const Vec3& linear_color,
                                                       float exposure_stops, bool use_reinhard) {
  Vec3 exposed = apply_exposure(linear_color, exposure_stops);
  if (use_reinhard) {
    exposed = reinhard_tonemap(exposed);
  }
  return linear_to_srgb(exposed);
}

} // namespace raypalette
