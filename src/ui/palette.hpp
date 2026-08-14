#pragma once

#include "math/vec3.hpp"

#include <string>

namespace raypalette::ui {

constexpr std::size_t kMaximumPaletteColors = 16;

struct PaletteColor {
  Vec3 color;
  std::string hex;
  float u = 0.0f;
  float v = 0.0f;
};

std::string color_to_hex(const Vec3& color);
bool same_palette_color(const Vec3& left, const Vec3& right);

} // namespace raypalette::ui
