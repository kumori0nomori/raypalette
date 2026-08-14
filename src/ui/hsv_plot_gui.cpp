#include "ui/hsv_plot_gui.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace raypalette::ui {

HsvScreenPoint project_hsv_position(const Vec3& position, const ImVec2& center, float scale,
                                    const HsvPlotView& view) {
  const Vec3 rotated = rotate_hsv_plot_position(position, view);
  return {{center.x + rotated.x * scale, center.y - rotated.y * scale}, rotated.z};
}

ImU32 hsv_plot_color(const Vec3& color, float alpha) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, alpha));
}

void draw_hsv_point_cloud(const HsvPlotCanvas& canvas, const std::vector<HsvPlotPoint>& points) {
  struct ProjectedPlotPoint {
    HsvScreenPoint screen;
    Vec3 color;
  };
  std::vector<ProjectedPlotPoint> projected_points;
  projected_points.reserve(points.size());
  for (const HsvPlotPoint& point : points) {
    projected_points.push_back({canvas.project(point.cylinder_position), point.display_color});
  }
  std::sort(projected_points.begin(), projected_points.end(),
            [](const ProjectedPlotPoint& left, const ProjectedPlotPoint& right) {
              return left.screen.depth < right.screen.depth;
            });
  for (const ProjectedPlotPoint& point : projected_points) {
    canvas.draw_list->AddCircleFilled(point.screen.position, 2.5f,
                                      hsv_plot_color(point.color, 0.62f));
  }
}

namespace {

void draw_hsv_marker(const HsvPlotCanvas& canvas, const char* label, const Vec3& color,
                     bool diamond, float radius) {
  const HsvScreenPoint screen = canvas.project(hsv_cylinder_position(srgb_to_hsv(color)));
  const ImU32 marker_color = hsv_plot_color(color);
  const ImU32 outline_color = ImGui::GetColorU32(ImGuiCol_Text);
  if (diamond) {
    canvas.draw_list->AddQuadFilled(ImVec2(screen.position.x, screen.position.y - radius),
                                    ImVec2(screen.position.x + radius, screen.position.y),
                                    ImVec2(screen.position.x, screen.position.y + radius),
                                    ImVec2(screen.position.x - radius, screen.position.y),
                                    marker_color);
    canvas.draw_list->AddQuad(ImVec2(screen.position.x, screen.position.y - radius),
                              ImVec2(screen.position.x + radius, screen.position.y),
                              ImVec2(screen.position.x, screen.position.y + radius),
                              ImVec2(screen.position.x - radius, screen.position.y), outline_color,
                              1.5f);
  } else {
    canvas.draw_list->AddCircleFilled(screen.position, radius, marker_color);
    canvas.draw_list->AddCircle(screen.position, radius, outline_color, 16, 1.5f);
  }
  canvas.draw_list->AddText(ImVec2(screen.position.x + radius + 3.0f, screen.position.y - 7.0f),
                            outline_color, label);
}

} // namespace

void draw_hsv_reference_markers(const HsvPlotCanvas& canvas, const Vec3& sphere_color,
                                const Vec3& light_color, float light_energy,
                                float light_energy_maximum,
                                const std::vector<PaletteColor>& palette,
                                int selected_palette_index) {
  draw_hsv_marker(canvas, "Sphere", sphere_color, true, 7.0f);
  const float normalized_energy =
      light_energy_maximum > 0.0f
          ? std::log1pf(std::max(0.0f, light_energy)) / std::log1pf(light_energy_maximum)
          : 0.0f;
  draw_hsv_marker(canvas, "Light", light_color, false, 5.0f + 6.0f * normalized_energy);
  for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
    const HsvScreenPoint screen =
        canvas.project(hsv_cylinder_position(srgb_to_hsv(palette[index].color)));
    const float radius = selected_palette_index == index ? 7.0f : 5.0f;
    const ImU32 outline_color = ImGui::GetColorU32(ImGuiCol_Text);
    canvas.draw_list->AddRectFilled(ImVec2(screen.position.x - radius, screen.position.y - radius),
                                    ImVec2(screen.position.x + radius, screen.position.y + radius),
                                    hsv_plot_color(palette[index].color));
    canvas.draw_list->AddRect(ImVec2(screen.position.x - radius, screen.position.y - radius),
                              ImVec2(screen.position.x + radius, screen.position.y + radius),
                              outline_color, 0.0f, 0, 1.5f);
    const std::string label = "P" + std::to_string(index);
    canvas.draw_list->AddText(ImVec2(screen.position.x + radius + 3.0f, screen.position.y - 7.0f),
                              outline_color, label.c_str());
  }
}

} // namespace raypalette::ui
