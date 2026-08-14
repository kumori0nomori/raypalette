#pragma once

#include "ui/hsv_plot.hpp"

#include "imgui.h"

namespace raypalette::ui {

struct HsvScreenPoint {
  ImVec2 position;
  float depth = 0.0f;
};

HsvScreenPoint project_hsv_position(const Vec3& position, const ImVec2& center, float scale,
                                    const HsvPlotView& view);
ImU32 hsv_plot_color(const Vec3& color, float alpha = 1.0f);

} // namespace raypalette::ui
