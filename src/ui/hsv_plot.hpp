#pragma once

#include "math/vec3.hpp"

namespace raypalette::ui {

struct HsvPlotView {
  float yaw = -0.65f;
  float pitch = 0.35f;
  float zoom = 1.0f;
};

Vec3 rotate_hsv_plot_position(const Vec3& position, const HsvPlotView& view);
Vec3 cylinder_position_at_hue(float hue, float saturation, float value);
float hue_distance(float first, float second);

} // namespace raypalette::ui
