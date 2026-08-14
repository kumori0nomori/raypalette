#include "ui/palette.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace raypalette::ui {

std::string color_to_hex(const Vec3& color) {
  const auto to_byte = [](float value) {
    return static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f));
  };

  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", to_byte(color.x), to_byte(color.y),
                to_byte(color.z));
  return buffer;
}

bool same_palette_color(const Vec3& left, const Vec3& right) {
  return color_to_hex(left) == color_to_hex(right);
}

} // namespace raypalette::ui
