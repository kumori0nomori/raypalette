#pragma once

#include "ui/hsv_plot.hpp"
#include "ui/palette.hpp"

#include "imgui.h"

#include <vector>

namespace raypalette::ui {

struct HsvScreenPoint {
  ImVec2 position;
  float depth = 0.0f;
};

HsvScreenPoint project_hsv_position(const Vec3& position, const ImVec2& center, float scale,
                                    const HsvPlotView& view);

struct HsvPlotCanvas {
  ImDrawList* draw_list;
  ImVec2 minimum;
  ImVec2 maximum;
  ImVec2 center;
  float scale;
  const HsvPlotView& view;

  HsvScreenPoint project(const Vec3& position) const {
    return project_hsv_position(position, center, scale, view);
  }
};

ImU32 hsv_plot_color(const Vec3& color, float alpha = 1.0f);

void draw_hsv_point_cloud(const HsvPlotCanvas& canvas, const std::vector<HsvPlotPoint>& points);
void draw_hsv_reference_markers(const HsvPlotCanvas& canvas, const Vec3& sphere_color,
                                const Vec3& light_color, float light_energy,
                                float light_energy_maximum,
                                const std::vector<PaletteColor>& palette,
                                int selected_palette_index);

} // namespace raypalette::ui
