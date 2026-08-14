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

void draw_hsv_base_color_wheel(const HsvPlotCanvas& canvas) {
  constexpr int kRingSegments = 32;
  constexpr int kColorWheelRings = 12;
  constexpr float kTwoPi = 6.28318530717958647692f;
  for (int segment = 0; segment < kRingSegments; ++segment) {
    const float hue_a = static_cast<float>(segment) / kRingSegments;
    const float hue_b = static_cast<float>(segment + 1) / kRingSegments;
    const float angle_a = kTwoPi * hue_a;
    const float angle_b = kTwoPi * hue_b;
    for (int ring = 0; ring < kColorWheelRings; ++ring) {
      const float inner_saturation = static_cast<float>(ring) / kColorWheelRings;
      const float outer_saturation = static_cast<float>(ring + 1) / kColorWheelRings;
      const HsvScreenPoint inner_a = canvas.project(
          {inner_saturation * std::cos(angle_a), 0.0f, inner_saturation * std::sin(angle_a)});
      const HsvScreenPoint inner_b = canvas.project(
          {inner_saturation * std::cos(angle_b), 0.0f, inner_saturation * std::sin(angle_b)});
      const HsvScreenPoint outer_a = canvas.project(
          {outer_saturation * std::cos(angle_a), 0.0f, outer_saturation * std::sin(angle_a)});
      const HsvScreenPoint outer_b = canvas.project(
          {outer_saturation * std::cos(angle_b), 0.0f, outer_saturation * std::sin(angle_b)});
      const Vec3 color = hsv_to_srgb({(hue_a + hue_b) * 0.5f, outer_saturation, 1.0f});
      canvas.draw_list->AddQuadFilled(inner_a.position, inner_b.position, outer_b.position,
                                      outer_a.position, hsv_plot_color(color));
    }
  }
}

void draw_hsv_cylinder_guides(const HsvPlotCanvas& canvas, ImU32 guide_color) {
  constexpr int kRingSegments = 32;
  constexpr float kTwoPi = 6.28318530717958647692f;
  for (int segment = 0; segment < kRingSegments; ++segment) {
    const float angle_a = kTwoPi * segment / kRingSegments;
    const float angle_b = kTwoPi * (segment + 1) / kRingSegments;
    const Vec3 lower_a{std::cos(angle_a), 0.0f, std::sin(angle_a)};
    const Vec3 lower_b{std::cos(angle_b), 0.0f, std::sin(angle_b)};
    const Vec3 upper_a{lower_a.x, 1.0f, lower_a.z};
    const Vec3 upper_b{lower_b.x, 1.0f, lower_b.z};
    const HsvScreenPoint lower_a_screen = canvas.project(lower_a);
    const HsvScreenPoint lower_b_screen = canvas.project(lower_b);
    const HsvScreenPoint upper_a_screen = canvas.project(upper_a);
    const HsvScreenPoint upper_b_screen = canvas.project(upper_b);
    canvas.draw_list->AddLine(lower_a_screen.position, lower_b_screen.position, guide_color, 1.0f);
    canvas.draw_list->AddLine(upper_a_screen.position, upper_b_screen.position, guide_color, 1.0f);
    if (segment % 4 == 0) {
      canvas.draw_list->AddLine(lower_a_screen.position, upper_a_screen.position, guide_color,
                                1.0f);
    }
  }
}

void draw_hsv_section_plane(const HsvPlotCanvas& canvas, const HsvHueSection& section) {
  const HsvScreenPoint bottom_center = canvas.project({0.0f, 0.0f, 0.0f});
  const HsvScreenPoint bottom_edge =
      canvas.project(cylinder_position_at_hue(section.hue, 1.0f, 0.0f));
  const HsvScreenPoint top_edge = canvas.project(cylinder_position_at_hue(section.hue, 1.0f, 1.0f));
  const HsvScreenPoint top_center = canvas.project({0.0f, 1.0f, 0.0f});
  const ImU32 section_color = hsv_plot_color(hsv_to_srgb({section.hue, 1.0f, 1.0f}), 0.22f);
  const ImU32 outline_color = ImGui::GetColorU32(ImGuiCol_Text);
  canvas.draw_list->AddQuadFilled(bottom_center.position, bottom_edge.position, top_edge.position,
                                  top_center.position, section_color);
  canvas.draw_list->AddLine(bottom_center.position, bottom_edge.position, outline_color, 2.0f);
  canvas.draw_list->AddLine(bottom_edge.position, top_edge.position, outline_color, 2.0f);
  canvas.draw_list->AddLine(top_edge.position, top_center.position, outline_color, 2.0f);
  const auto draw_boundary = [&](float hue) {
    const HsvScreenPoint lower = canvas.project(cylinder_position_at_hue(hue, 1.0f, 0.0f));
    const HsvScreenPoint upper = canvas.project(cylinder_position_at_hue(hue, 1.0f, 1.0f));
    canvas.draw_list->AddLine(lower.position, upper.position, outline_color, 1.0f);
  };
  draw_boundary(std::fmod(section.hue - section.half_width + 1.0f, 1.0f));
  draw_boundary(std::fmod(section.hue + section.half_width, 1.0f));
}

namespace {

void handle_hsv_view_input(bool hovered, HsvPlotView& view) {
  if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    const ImVec2 drag_delta = ImGui::GetIO().MouseDelta;
    view.yaw += drag_delta.x * 0.01f;
    view.pitch = std::clamp(view.pitch + drag_delta.y * 0.01f, -1.4f, 1.4f);
  }
  if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
    view.zoom = std::clamp(view.zoom + ImGui::GetIO().MouseWheel * 0.1f, 0.55f, 1.8f);
  }
}

void select_hue_at_mouse(const HsvPlotCanvas& canvas, HsvHueSection& section) {
  constexpr int kHueSelectionSamples = 360;
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  float nearest_distance_squared = 1.0e30f;
  float selected_hue = section.hue;
  for (int sample = 0; sample < kHueSelectionSamples; ++sample) {
    const float hue = static_cast<float>(sample) / kHueSelectionSamples;
    const HsvScreenPoint rim = canvas.project(cylinder_position_at_hue(hue, 1.0f, 0.0f));
    const float direction_x = rim.position.x - canvas.center.x;
    const float direction_y = rim.position.y - canvas.center.y;
    const float direction_length_squared = direction_x * direction_x + direction_y * direction_y;
    if (direction_length_squared <= 1.0e-6f) {
      continue;
    }
    const float mouse_x = mouse.x - canvas.center.x;
    const float mouse_y = mouse.y - canvas.center.y;
    const float projection = std::clamp(
        (mouse_x * direction_x + mouse_y * direction_y) / direction_length_squared, 0.0f, 1.0f);
    const float closest_x = canvas.center.x + projection * direction_x;
    const float closest_y = canvas.center.y + projection * direction_y;
    const float distance_x = mouse.x - closest_x;
    const float distance_y = mouse.y - closest_y;
    const float distance_squared = distance_x * distance_x + distance_y * distance_y;
    if (distance_squared < nearest_distance_squared) {
      nearest_distance_squared = distance_squared;
      selected_hue = hue;
    }
  }
  section.hue = selected_hue;
}

} // namespace

void draw_hsv_space(const std::vector<HsvPlotPoint>& points, const Vec3& sphere_color,
                    const Vec3& light_color, float light_energy, float light_energy_maximum,
                    HsvHueSection& section, const std::vector<PaletteColor>& palette,
                    int selected_palette_index, HsvPlotView& view) {
  if (ImGui::Button("Reset HSV view")) {
    view = {};
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Drag to orbit. Scroll to zoom.");

  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float canvas_size = std::max(240.0f, std::min(420.0f, available.x));
  ImGui::InvisibleButton("##hsv-space-canvas", ImVec2(canvas_size, canvas_size),
                         ImGuiButtonFlags_MouseButtonLeft);
  const ImVec2 canvas_min = ImGui::GetItemRectMin();
  const ImVec2 canvas_max = ImGui::GetItemRectMax();
  const ImVec2 center((canvas_min.x + canvas_max.x) * 0.5f, (canvas_min.y + canvas_max.y) * 0.5f);
  const bool hovered = ImGui::IsItemHovered();
  handle_hsv_view_input(hovered, view);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(canvas_min, canvas_max, ImGui::GetColorU32(ImGuiCol_FrameBg));
  draw_list->AddRect(canvas_min, canvas_max, ImGui::GetColorU32(ImGuiCol_Border));
  const float scale = canvas_size * 0.31f * view.zoom;
  const ImU32 guide_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  const HsvPlotCanvas canvas{draw_list, canvas_min, canvas_max, center, scale, view};
  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    select_hue_at_mouse(canvas, section);
  }
  draw_hsv_base_color_wheel(canvas);
  draw_hsv_cylinder_guides(canvas, guide_color);
  draw_hsv_section_plane(canvas, section);
  draw_hsv_point_cloud(canvas, points);
  draw_hsv_reference_markers(canvas, sphere_color, light_color, light_energy, light_energy_maximum,
                             palette, selected_palette_index);
  draw_list->AddText(ImVec2(canvas_min.x + 8.0f, canvas_min.y + 8.0f), guide_color,
                     "HSV cylinder: H angle, S radius, V height");
  draw_list->AddText(ImVec2(canvas_min.x + 8.0f, canvas_max.y - 22.0f), guide_color,
                     "Base color circle: V = 1");
}

} // namespace raypalette::ui
