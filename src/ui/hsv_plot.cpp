#include "ui/hsv_plot.hpp"

#include <algorithm>
#include <cmath>

namespace raypalette::ui {

Vec3 rotate_hsv_plot_position(const Vec3& position, const HsvPlotView& view) {
  const float yaw_cosine = std::cos(view.yaw);
  const float yaw_sine = std::sin(view.yaw);
  const float pitch_cosine = std::cos(view.pitch);
  const float pitch_sine = std::sin(view.pitch);
  const float yaw_x = yaw_cosine * position.x - yaw_sine * position.z;
  const float yaw_z = yaw_sine * position.x + yaw_cosine * position.z;
  return {yaw_x, pitch_cosine * (position.y - 0.5f) - pitch_sine * yaw_z,
          pitch_sine * (position.y - 0.5f) + pitch_cosine * yaw_z};
}

Vec3 cylinder_position_at_hue(float hue, float saturation, float value) {
  constexpr float kTwoPi = 6.28318530717958647692f;
  const float angle = kTwoPi * hue;
  return {saturation * std::cos(angle), value, saturation * std::sin(angle)};
}

float hue_distance(float first, float second) {
  const float direct_distance = std::fabs(first - second);
  return std::min(direct_distance, 1.0f - direct_distance);
}

} // namespace raypalette::ui
