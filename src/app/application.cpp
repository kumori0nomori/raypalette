#include "math/color.hpp"
#include "render/renderer.hpp"
#include "ui/hsv_plot.hpp"
#include "ui/hsv_plot_gui.hpp"
#include "ui/palette.hpp"
#include "ui/texture.hpp"

#include <GLFW/glfw3.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct WindowSize {
  float width;
  float height;
};

struct GuiLayout {
  WindowSize main_window{1100.0f, 760.0f};
  WindowSize controls_panel{360.0f, 750.0f};
  WindowSize preview_panel{600.0f, 750.0f};
  WindowSize hsv_space_panel{480.0f, 750.0f};
};

constexpr GuiLayout kDefaultGuiLayout{};

raypalette::Vec3& light_color(raypalette::Light& light) {
  switch (light.type) {
  case raypalette::LightType::Point:
    return light.point.color;
  case raypalette::LightType::Directional:
    return light.directional.color;
  case raypalette::LightType::RectArea:
    return light.area.color;
  }
  return light.point.color;
}

float& light_energy(raypalette::Light& light) {
  switch (light.type) {
  case raypalette::LightType::Point:
    return light.point.radiant_intensity;
  case raypalette::LightType::Directional:
    return light.directional.irradiance;
  case raypalette::LightType::RectArea:
    return light.area.radiance;
  }
  return light.point.radiant_intensity;
}

struct LightEnergyUi {
  const char* label;
  float minimum;
  float maximum;
};

LightEnergyUi light_energy_ui(raypalette::LightType type) {
  switch (type) {
  case raypalette::LightType::Point:
    return {"Radiant intensity", 0.0f, 500.0f};
  case raypalette::LightType::Directional:
    return {"Sun irradiance", 0.0f, 10.0f};
  case raypalette::LightType::RectArea:
    return {"Area radiance", 0.0f, 50.0f};
  }
  return {"Light energy", 0.0f, 1.0f};
}

raypalette::Vec3 sphere_material_color(const raypalette::Material& material) {
  switch (material.type) {
  case raypalette::MaterialType::Dielectric:
    return material.transmission_color;
  case raypalette::MaterialType::Emissive:
    return material.emission_color;
  case raypalette::MaterialType::Diffuse:
  case raypalette::MaterialType::Metal:
    return material.base_color;
  }
  return material.base_color;
}

struct HsvPlotCanvas {
  ImDrawList* draw_list;
  ImVec2 minimum;
  ImVec2 maximum;
  ImVec2 center;
  float scale;
  const raypalette::ui::HsvPlotView& view;

  raypalette::ui::HsvScreenPoint project(const raypalette::Vec3& position) const {
    return raypalette::ui::project_hsv_position(position, center, scale, view);
  }
};

void draw_hsv_point_cloud(const HsvPlotCanvas& canvas,
                          const std::vector<raypalette::ui::HsvPlotPoint>& points) {
  struct ProjectedPlotPoint {
    raypalette::ui::HsvScreenPoint screen;
    raypalette::Vec3 color;
  };
  std::vector<ProjectedPlotPoint> projected_points;
  projected_points.reserve(points.size());
  for (const raypalette::ui::HsvPlotPoint& point : points) {
    projected_points.push_back({canvas.project(point.cylinder_position), point.display_color});
  }
  std::sort(projected_points.begin(), projected_points.end(),
            [](const ProjectedPlotPoint& left, const ProjectedPlotPoint& right) {
              return left.screen.depth < right.screen.depth;
            });
  for (const ProjectedPlotPoint& point : projected_points) {
    canvas.draw_list->AddCircleFilled(point.screen.position, 2.5f,
                                      raypalette::ui::hsv_plot_color(point.color, 0.62f));
  }
}

void draw_hsv_marker(const HsvPlotCanvas& canvas, const char* label, const raypalette::Vec3& color,
                     bool diamond, float radius) {
  const raypalette::ui::HsvScreenPoint screen =
      canvas.project(raypalette::hsv_cylinder_position(raypalette::srgb_to_hsv(color)));
  const ImU32 marker_color = raypalette::ui::hsv_plot_color(color);
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

void draw_hsv_reference_markers(const HsvPlotCanvas& canvas, const raypalette::Vec3& sphere_color,
                                const raypalette::Vec3& light_color_value, float light_energy_value,
                                float light_energy_maximum,
                                const std::vector<raypalette::ui::PaletteColor>& palette,
                                int selected_palette_index) {
  draw_hsv_marker(canvas, "Sphere", sphere_color, true, 7.0f);
  const float normalized_energy =
      light_energy_maximum > 0.0f
          ? std::log1pf(std::max(0.0f, light_energy_value)) / std::log1pf(light_energy_maximum)
          : 0.0f;
  draw_hsv_marker(canvas, "Light", light_color_value, false, 5.0f + 6.0f * normalized_energy);
  for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
    const raypalette::ui::HsvScreenPoint screen = canvas.project(
        raypalette::hsv_cylinder_position(raypalette::srgb_to_hsv(palette[index].color)));
    const float radius = selected_palette_index == index ? 7.0f : 5.0f;
    const ImU32 outline_color = ImGui::GetColorU32(ImGuiCol_Text);
    canvas.draw_list->AddRectFilled(ImVec2(screen.position.x - radius, screen.position.y - radius),
                                    ImVec2(screen.position.x + radius, screen.position.y + radius),
                                    raypalette::ui::hsv_plot_color(palette[index].color));
    canvas.draw_list->AddRect(ImVec2(screen.position.x - radius, screen.position.y - radius),
                              ImVec2(screen.position.x + radius, screen.position.y + radius),
                              outline_color, 0.0f, 0, 1.5f);
    const std::string label = "P" + std::to_string(index);
    canvas.draw_list->AddText(ImVec2(screen.position.x + radius + 3.0f, screen.position.y - 7.0f),
                              outline_color, label.c_str());
  }
}

void handle_hsv_view_input(bool hovered, raypalette::ui::HsvPlotView& view) {
  if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    const ImVec2 drag_delta = ImGui::GetIO().MouseDelta;
    view.yaw += drag_delta.x * 0.01f;
    view.pitch = std::clamp(view.pitch + drag_delta.y * 0.01f, -1.4f, 1.4f);
  }
  if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
    view.zoom = std::clamp(view.zoom + ImGui::GetIO().MouseWheel * 0.1f, 0.55f, 1.8f);
  }
}

void select_hue_at_mouse(const HsvPlotCanvas& canvas, raypalette::ui::HsvHueSection& section) {
  constexpr int kHueSelectionSamples = 360;
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  float nearest_distance_squared = 1.0e30f;
  float selected_hue = section.hue;
  for (int sample = 0; sample < kHueSelectionSamples; ++sample) {
    const float hue = static_cast<float>(sample) / kHueSelectionSamples;
    const raypalette::ui::HsvScreenPoint rim =
        canvas.project(raypalette::hsv_cylinder_position({hue, 1.0f, 0.0f}));
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
      const raypalette::ui::HsvScreenPoint inner_a = canvas.project(
          {inner_saturation * std::cos(angle_a), 0.0f, inner_saturation * std::sin(angle_a)});
      const raypalette::ui::HsvScreenPoint inner_b = canvas.project(
          {inner_saturation * std::cos(angle_b), 0.0f, inner_saturation * std::sin(angle_b)});
      const raypalette::ui::HsvScreenPoint outer_a = canvas.project(
          {outer_saturation * std::cos(angle_a), 0.0f, outer_saturation * std::sin(angle_a)});
      const raypalette::ui::HsvScreenPoint outer_b = canvas.project(
          {outer_saturation * std::cos(angle_b), 0.0f, outer_saturation * std::sin(angle_b)});
      const raypalette::Vec3 color =
          raypalette::hsv_to_srgb({(hue_a + hue_b) * 0.5f, outer_saturation, 1.0f});
      canvas.draw_list->AddQuadFilled(inner_a.position, inner_b.position, outer_b.position,
                                      outer_a.position, raypalette::ui::hsv_plot_color(color));
    }
  }
}

void draw_hsv_cylinder_guides(const HsvPlotCanvas& canvas, ImU32 guide_color) {
  constexpr int kRingSegments = 32;
  constexpr float kTwoPi = 6.28318530717958647692f;
  for (int segment = 0; segment < kRingSegments; ++segment) {
    const float angle_a = kTwoPi * segment / kRingSegments;
    const float angle_b = kTwoPi * (segment + 1) / kRingSegments;
    const raypalette::Vec3 lower_a{std::cos(angle_a), 0.0f, std::sin(angle_a)};
    const raypalette::Vec3 lower_b{std::cos(angle_b), 0.0f, std::sin(angle_b)};
    const raypalette::Vec3 upper_a{lower_a.x, 1.0f, lower_a.z};
    const raypalette::Vec3 upper_b{lower_b.x, 1.0f, lower_b.z};
    const raypalette::ui::HsvScreenPoint lower_a_screen = canvas.project(lower_a);
    const raypalette::ui::HsvScreenPoint lower_b_screen = canvas.project(lower_b);
    const raypalette::ui::HsvScreenPoint upper_a_screen = canvas.project(upper_a);
    const raypalette::ui::HsvScreenPoint upper_b_screen = canvas.project(upper_b);
    canvas.draw_list->AddLine(lower_a_screen.position, lower_b_screen.position, guide_color, 1.0f);
    canvas.draw_list->AddLine(upper_a_screen.position, upper_b_screen.position, guide_color, 1.0f);
    if (segment % 4 == 0) {
      canvas.draw_list->AddLine(lower_a_screen.position, upper_a_screen.position, guide_color,
                                1.0f);
    }
  }
}

void draw_hsv_section_boundary(const HsvPlotCanvas& canvas, float hue, ImU32 outline_color) {
  const raypalette::ui::HsvScreenPoint lower =
      canvas.project(raypalette::ui::cylinder_position_at_hue(hue, 1.0f, 0.0f));
  const raypalette::ui::HsvScreenPoint upper =
      canvas.project(raypalette::ui::cylinder_position_at_hue(hue, 1.0f, 1.0f));
  canvas.draw_list->AddLine(lower.position, upper.position, outline_color, 1.0f);
}

void draw_hsv_section_plane(const HsvPlotCanvas& canvas,
                            const raypalette::ui::HsvHueSection& section) {
  const raypalette::ui::HsvScreenPoint bottom_center = canvas.project({0.0f, 0.0f, 0.0f});
  const raypalette::ui::HsvScreenPoint bottom_edge =
      canvas.project(raypalette::ui::cylinder_position_at_hue(section.hue, 1.0f, 0.0f));
  const raypalette::ui::HsvScreenPoint top_edge =
      canvas.project(raypalette::ui::cylinder_position_at_hue(section.hue, 1.0f, 1.0f));
  const raypalette::ui::HsvScreenPoint top_center = canvas.project({0.0f, 1.0f, 0.0f});
  const ImU32 section_color =
      raypalette::ui::hsv_plot_color(raypalette::hsv_to_srgb({section.hue, 1.0f, 1.0f}), 0.22f);
  const ImU32 outline_color = ImGui::GetColorU32(ImGuiCol_Text);
  canvas.draw_list->AddQuadFilled(bottom_center.position, bottom_edge.position, top_edge.position,
                                  top_center.position, section_color);
  canvas.draw_list->AddLine(bottom_center.position, bottom_edge.position, outline_color, 2.0f);
  canvas.draw_list->AddLine(bottom_edge.position, top_edge.position, outline_color, 2.0f);
  canvas.draw_list->AddLine(top_edge.position, top_center.position, outline_color, 2.0f);
  draw_hsv_section_boundary(canvas, std::fmod(section.hue - section.half_width + 1.0f, 1.0f),
                            outline_color);
  draw_hsv_section_boundary(canvas, std::fmod(section.hue + section.half_width, 1.0f),
                            outline_color);
}

void draw_hsv_space(const std::vector<raypalette::ui::HsvPlotPoint>& points,
                    const raypalette::Vec3& sphere_color, const raypalette::Vec3& light_color_value,
                    float light_energy_value, float light_energy_maximum,
                    raypalette::ui::HsvHueSection& section,
                    const std::vector<raypalette::ui::PaletteColor>& palette,
                    int selected_palette_index, raypalette::ui::HsvPlotView& view) {
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
  draw_hsv_reference_markers(canvas, sphere_color, light_color_value, light_energy_value,
                             light_energy_maximum, palette, selected_palette_index);
  draw_list->AddText(ImVec2(canvas_min.x + 8.0f, canvas_min.y + 8.0f), guide_color,
                     "HSV cylinder: H angle, S radius, V height");
  draw_list->AddText(ImVec2(canvas_min.x + 8.0f, canvas_max.y - 22.0f), guide_color,
                     "Base color circle: V = 1");
}

void draw_hsv_hue_section(const std::vector<raypalette::ui::HsvPlotPoint>& points,
                          raypalette::ui::HsvHueSection& section,
                          const raypalette::Vec3& sphere_color,
                          const raypalette::Vec3& light_color_value,
                          const std::vector<raypalette::ui::PaletteColor>& palette,
                          int selected_palette_index) {
  ImGui::Separator();
  ImGui::Text("Hue section");
  const float section_width = std::max(240.0f, std::min(420.0f, ImGui::GetContentRegionAvail().x));
  constexpr float kHueBarHeight = 18.0f;
  ImGui::InvisibleButton("##hsv-section-hue", ImVec2(section_width, kHueBarHeight),
                         ImGuiButtonFlags_MouseButtonLeft);
  const ImVec2 hue_bar_min = ImGui::GetItemRectMin();
  const ImVec2 hue_bar_max = ImGui::GetItemRectMax();
  if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    section.hue =
        std::clamp((ImGui::GetIO().MousePos.x - hue_bar_min.x) / section_width, 0.0f, 1.0f);
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  constexpr int kHueBarSegments = 48;
  for (int segment = 0; segment < kHueBarSegments; ++segment) {
    const float x0 = hue_bar_min.x + section_width * segment / kHueBarSegments;
    const float x1 = hue_bar_min.x + section_width * (segment + 1) / kHueBarSegments;
    draw_list->AddRectFilled(ImVec2(x0, hue_bar_min.y), ImVec2(x1, hue_bar_max.y),
                             raypalette::ui::hsv_plot_color(raypalette::hsv_to_srgb(
                                 {static_cast<float>(segment) / kHueBarSegments, 1.0f, 1.0f})));
  }
  const float hue_marker_x = hue_bar_min.x + section.hue * section_width;
  draw_list->AddLine(ImVec2(hue_marker_x, hue_bar_min.y - 2.0f),
                     ImVec2(hue_marker_x, hue_bar_max.y + 2.0f), ImGui::GetColorU32(ImGuiCol_Text),
                     2.0f);

  constexpr int kSaturationSteps = 48;
  constexpr int kValueSteps = 32;
  const float section_height = section_width * 0.52f;
  ImGui::InvisibleButton("##hsv-section-plane", ImVec2(section_width, section_height));
  const ImVec2 plane_min = ImGui::GetItemRectMin();
  const ImVec2 plane_max = ImGui::GetItemRectMax();
  for (int value_step = 0; value_step < kValueSteps; ++value_step) {
    const float value = 1.0f - (static_cast<float>(value_step) + 0.5f) / kValueSteps;
    const float y0 = plane_min.y + section_height * value_step / kValueSteps;
    const float y1 = plane_min.y + section_height * (value_step + 1) / kValueSteps;
    for (int saturation_step = 0; saturation_step < kSaturationSteps; ++saturation_step) {
      const float saturation = (static_cast<float>(saturation_step) + 0.5f) / kSaturationSteps;
      const float x0 = plane_min.x + section_width * saturation_step / kSaturationSteps;
      const float x1 = plane_min.x + section_width * (saturation_step + 1) / kSaturationSteps;
      draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                               raypalette::ui::hsv_plot_color(
                                   raypalette::hsv_to_srgb({section.hue, saturation, value})));
    }
  }
  draw_list->AddRect(plane_min, plane_max, ImGui::GetColorU32(ImGuiCol_Border));
  for (const raypalette::ui::HsvPlotPoint& point : points) {
    if (raypalette::ui::hue_distance(point.hsv.hue, section.hue) > section.half_width) {
      continue;
    }
    const ImVec2 position(plane_min.x + point.hsv.saturation * section_width,
                          plane_max.y - point.hsv.value * section_height);
    draw_list->AddCircleFilled(position, 3.0f, raypalette::ui::hsv_plot_color(point.display_color));
    draw_list->AddCircle(position, 3.0f, ImGui::GetColorU32(ImGuiCol_Text), 12, 1.0f);
  }
  const auto draw_section_marker = [&](const char* label, const raypalette::Vec3& color,
                                       bool diamond, float radius) {
    const raypalette::Hsv hsv = raypalette::srgb_to_hsv(color);
    if (raypalette::ui::hue_distance(hsv.hue, section.hue) > section.half_width) {
      return;
    }
    const ImVec2 position(plane_min.x + hsv.saturation * section_width,
                          plane_max.y - hsv.value * section_height);
    const ImU32 outline_color = ImGui::GetColorU32(ImGuiCol_Text);
    if (diamond) {
      draw_list->AddQuadFilled(
          ImVec2(position.x, position.y - radius), ImVec2(position.x + radius, position.y),
          ImVec2(position.x, position.y + radius), ImVec2(position.x - radius, position.y),
          raypalette::ui::hsv_plot_color(color));
      draw_list->AddQuad(ImVec2(position.x, position.y - radius),
                         ImVec2(position.x + radius, position.y),
                         ImVec2(position.x, position.y + radius),
                         ImVec2(position.x - radius, position.y), outline_color, 1.5f);
    } else {
      draw_list->AddCircleFilled(position, radius, raypalette::ui::hsv_plot_color(color));
      draw_list->AddCircle(position, radius, outline_color, 16, 1.5f);
    }
    draw_list->AddText(ImVec2(position.x + radius + 3.0f, position.y - 7.0f), outline_color, label);
  };
  draw_section_marker("Sphere", sphere_color, true, 7.0f);
  draw_section_marker("Light", light_color_value, false, 6.0f);
  for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
    const raypalette::Hsv hsv = raypalette::srgb_to_hsv(palette[index].color);
    if (raypalette::ui::hue_distance(hsv.hue, section.hue) > section.half_width) {
      continue;
    }
    const ImVec2 position(plane_min.x + hsv.saturation * section_width,
                          plane_max.y - hsv.value * section_height);
    const float radius = selected_palette_index == index ? 6.0f : 4.0f;
    const ImU32 outline_color = ImGui::GetColorU32(ImGuiCol_Text);
    draw_list->AddRectFilled(ImVec2(position.x - radius, position.y - radius),
                             ImVec2(position.x + radius, position.y + radius),
                             raypalette::ui::hsv_plot_color(palette[index].color));
    draw_list->AddRect(ImVec2(position.x - radius, position.y - radius),
                       ImVec2(position.x + radius, position.y + radius), outline_color, 0.0f, 0,
                       1.5f);
    const std::string label = "P" + std::to_string(index);
    draw_list->AddText(ImVec2(position.x + radius + 3.0f, position.y - 7.0f), outline_color,
                       label.c_str());
  }
  draw_list->AddText(ImVec2(plane_min.x + 5.0f, plane_min.y + 5.0f),
                     ImGui::GetColorU32(ImGuiCol_TextDisabled), "V");
  draw_list->AddText(ImVec2(plane_max.x - 12.0f, plane_max.y - 20.0f),
                     ImGui::GetColorU32(ImGuiCol_TextDisabled), "S");
}

struct MaterialUi {
  const char* const* labels;
  int label_count;
};

constexpr const char* kMaterialLabels[] = {"Diffuse", "Metal", "Glass", "Emissive"};
constexpr MaterialUi kSphereMaterialUi{kMaterialLabels,
                                       static_cast<int>(std::size(kMaterialLabels))};

int material_type_index(raypalette::MaterialType type) {
  switch (type) {
  case raypalette::MaterialType::Diffuse:
    return 0;
  case raypalette::MaterialType::Metal:
    return 1;
  case raypalette::MaterialType::Dielectric:
    return 2;
  case raypalette::MaterialType::Emissive:
    return 3;
  }
  return 0;
}

raypalette::MaterialType material_type_from_index(int index) {
  switch (index) {
  case 1:
    return raypalette::MaterialType::Metal;
  case 2:
    return raypalette::MaterialType::Dielectric;
  case 3:
    return raypalette::MaterialType::Emissive;
  default:
    return raypalette::MaterialType::Diffuse;
  }
}

} // namespace

int main() {
  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  const GuiLayout& layout = kDefaultGuiLayout;
  GLFWwindow* window =
      glfwCreateWindow(static_cast<int>(layout.main_window.width),
                       static_cast<int>(layout.main_window.height), "RayPalette", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  raypalette::Scene scene = raypalette::make_default_scene();
  raypalette::PolarCoordinates light_polar{4.0f, 35.0f, 45.0f};
  int light_type_index = static_cast<int>(scene.light.type);
  raypalette::RenderSettings settings{/*width=*/512,
                                      /*height=*/512,
                                      /*minimum_distance=*/0.001f,
                                      /*samples_per_pixel=*/4,
                                      /*light_samples_per_frame=*/4,
                                      /*target_samples_per_pixel=*/64,
                                      /*max_bounces=*/8};
  raypalette::ui::DisplaySettings display_settings;
  float camera_distance = 5.0f;
  raypalette::Camera camera = raypalette::make_default_camera(1.0f, camera_distance);
  raypalette::Renderer renderer;
  raypalette::ui::Texture texture;
  raypalette::Image image;
  std::vector<raypalette::ui::PaletteColor> palette;
  std::vector<raypalette::ui::HsvPlotPoint> sphere_hsv_points;
  raypalette::ui::HsvPlotView hsv_plot_view;
  raypalette::ui::HsvHueSection hsv_hue_section;
  int selected_palette_index = -1;
  bool needs_render = true;
  bool needs_display_update = true;
  auto clear_palette = [&]() {
    palette.clear();
    selected_palette_index = -1;
  };
  auto request_render = [&]() {
    renderer.reset_accumulation();
    clear_palette();
    needs_render = true;
  };
  float settings_panel_right = 0.0f;
  auto begin_settings_panel = [&]() {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    settings_panel_right = start.x + ImGui::GetContentRegionAvail().x;
    ImGui::BeginGroup();
    return start;
  };
  auto end_settings_panel = [&](const ImVec2& start) {
    ImGui::EndGroup();
    const float panel_bottom = ImGui::GetItemRectMax().y;
    ImGui::GetWindowDrawList()->AddRect(ImVec2(start.x - 4.0f, start.y - 3.0f),
                                        ImVec2(settings_panel_right + 4.0f, panel_bottom + 3.0f),
                                        ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
  };

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(layout.controls_panel.width, layout.controls_panel.height),
                             ImGuiCond_FirstUseEver);
    ImGui::Begin("RayPalette Controls");
    if (ImGui::Button("Reset scene")) {
      scene = raypalette::make_default_scene();
      light_polar = {4.0f, 35.0f, 45.0f};
      light_type_index = static_cast<int>(scene.light.type);
      camera_distance = 5.0f;
      camera = raypalette::make_default_camera(1.0f, camera_distance);
      palette.clear();
      selected_palette_index = -1;
      request_render();
    }

    // Sphere and materials controls
    const ImVec2 sphere_panel_start = begin_settings_panel();
    if (ImGui::CollapsingHeader("Sphere & Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
      raypalette::Material& sphere_material = scene.materials[raypalette::kSphereMaterialIndex];
      int sphere_material_index = material_type_index(sphere_material.type);
      if (ImGui::Combo("Sphere material", &sphere_material_index, kSphereMaterialUi.labels,
                       kSphereMaterialUi.label_count)) {
        const raypalette::MaterialType previous_type = sphere_material.type;
        const raypalette::MaterialType next_type = material_type_from_index(sphere_material_index);
        if (next_type == raypalette::MaterialType::Dielectric &&
            previous_type != raypalette::MaterialType::Dielectric) {
          sphere_material.transmission_color = previous_type == raypalette::MaterialType::Emissive
                                                   ? sphere_material.emission_color
                                                   : sphere_material.base_color;
          sphere_material.base_color = {1.0f, 1.0f, 1.0f};
        } else if (next_type == raypalette::MaterialType::Emissive &&
                   previous_type != raypalette::MaterialType::Emissive) {
          sphere_material.emission_color = previous_type == raypalette::MaterialType::Dielectric
                                               ? sphere_material.transmission_color
                                               : sphere_material.base_color;
          sphere_material.base_color = {1.0f, 1.0f, 1.0f};
        } else if (previous_type == raypalette::MaterialType::Dielectric &&
                   next_type != raypalette::MaterialType::Dielectric) {
          sphere_material.base_color = sphere_material.transmission_color;
        } else if (previous_type == raypalette::MaterialType::Emissive &&
                   next_type != raypalette::MaterialType::Emissive) {
          sphere_material.base_color = sphere_material.emission_color;
        }
        sphere_material.type = next_type;
        request_render();
      }
      if (sphere_material.type == raypalette::MaterialType::Dielectric) {
        if (ImGui::ColorEdit3("Glass transmission", &sphere_material.transmission_color.x,
                              ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
          sphere_material.transmission_color.x =
              std::max(0.0f, sphere_material.transmission_color.x);
          sphere_material.transmission_color.y =
              std::max(0.0f, sphere_material.transmission_color.y);
          sphere_material.transmission_color.z =
              std::max(0.0f, sphere_material.transmission_color.z);
          request_render();
        }
      } else if (sphere_material.type == raypalette::MaterialType::Emissive) {
        if (ImGui::ColorEdit3("Sphere emission", &sphere_material.emission_color.x,
                              ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
          sphere_material.emission_color.x = std::max(0.0f, sphere_material.emission_color.x);
          sphere_material.emission_color.y = std::max(0.0f, sphere_material.emission_color.y);
          sphere_material.emission_color.z = std::max(0.0f, sphere_material.emission_color.z);
          request_render();
        }
      } else if (ImGui::ColorEdit3("Sphere color", &sphere_material.base_color.x,
                                   ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
        request_render();
      }
      if (scene.materials[raypalette::kSphereMaterialIndex].type ==
          raypalette::MaterialType::Metal) {
        if (ImGui::SliderFloat("Metal roughness",
                               &scene.materials[raypalette::kSphereMaterialIndex].roughness, 0.0f,
                               1.0f)) {
          request_render();
        }
      }
      if (scene.materials[raypalette::kSphereMaterialIndex].type ==
          raypalette::MaterialType::Dielectric) {
        if (ImGui::SliderFloat(
                "Glass absorption density",
                &scene.materials[raypalette::kSphereMaterialIndex].absorption_density, 0.0f,
                5.0f)) {
          request_render();
        }
        constexpr float kMinimumGlassIor = 1.01f;
        constexpr float kMaximumGlassIor = 3.0f;
        const float ior_log_denominator = std::log(kMaximumGlassIor);
        float ior_slider =
            std::log(std::max(kMinimumGlassIor, sphere_material.index_of_refraction)) /
            ior_log_denominator;
        const float minimum_ior_slider = std::log(kMinimumGlassIor) / ior_log_denominator;
        if (ImGui::SliderFloat("Glass IOR##slider", &ior_slider, minimum_ior_slider, 1.0f, "")) {
          sphere_material.index_of_refraction =
              1.0f + (std::exp(ior_slider * ior_log_denominator) - 1.0f);
          request_render();
        }
        ImGui::SameLine();
        ImGui::Text("IOR %.3f", sphere_material.index_of_refraction);
      }
      if (scene.materials[raypalette::kSphereMaterialIndex].type ==
          raypalette::MaterialType::Emissive) {
        if (ImGui::SliderFloat("Sphere emission strength",
                               &scene.materials[raypalette::kSphereMaterialIndex].emission_strength,
                               0.0f, 10.0f)) {
          request_render();
        }
      }
      if (ImGui::ColorEdit3("Floor color",
                            &scene.materials[raypalette::kFloorMaterialIndex].base_color.x,
                            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
        request_render();
      }
    }

    end_settings_panel(sphere_panel_start);

    // Light controls
    const ImVec2 light_panel_start = begin_settings_panel();
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
      const char* light_types[] = {"Point", "Rectangular area", "Directional (sun)"};
      if (ImGui::Combo("Light type", &light_type_index, light_types, IM_ARRAYSIZE(light_types))) {
        const raypalette::LightType type = static_cast<raypalette::LightType>(light_type_index);
        const raypalette::Vec3 color = light_color(scene.light);
        const float energy = light_energy(scene.light);
        if (type == raypalette::LightType::Point) {
          scene.light =
              raypalette::make_point_light(light_polar, scene.sphere.center, color, energy);
        } else if (type == raypalette::LightType::Directional) {
          scene.light = raypalette::make_directional_light(light_polar.theta_degrees,
                                                           light_polar.phi_degrees, color, energy);
        } else {
          scene.light = raypalette::make_rect_area_light(
              light_polar, scene.sphere.center, {0.0f, -1.0f, 0.0f}, scene.light.area.width,
              scene.light.area.height, color, energy);
        }
        request_render();
      }

      float light_color_values[3] = {light_color(scene.light).x, light_color(scene.light).y,
                                     light_color(scene.light).z};
      if (ImGui::ColorEdit3("Light color", light_color_values, ImGuiColorEditFlags_NoInputs)) {
        light_color(scene.light) = {std::max(0.0f, light_color_values[0]),
                                    std::max(0.0f, light_color_values[1]),
                                    std::max(0.0f, light_color_values[2])};
        request_render();
      }
      const LightEnergyUi energy_ui = light_energy_ui(scene.light.type);
      if (ImGui::SliderFloat(energy_ui.label, &light_energy(scene.light), energy_ui.minimum,
                             energy_ui.maximum)) {
        request_render();
      }
      if (scene.light.type == raypalette::LightType::RectArea) {
        float area_size = 0.5f * (scene.light.area.width + scene.light.area.height);
        if (ImGui::SliderFloat("Area size", &area_size, 0.1f, 20.0f)) {
          scene.light.area.width = area_size;
          scene.light.area.height = area_size;
          request_render();
        }
      }
      bool light_parameters_changed = false;
      if (scene.light.type != raypalette::LightType::Directional) {
        light_parameters_changed |=
            ImGui::SliderFloat("Light radius", &light_polar.radius, 0.1f, 20.0f);
      } else {
        ImGui::TextDisabled("Sun light has no radius or distance falloff.");
      }
      light_parameters_changed |=
          ImGui::SliderFloat("Light theta", &light_polar.theta_degrees, 0.0f, 90.0f);
      light_parameters_changed |=
          ImGui::SliderFloat("Light phi", &light_polar.phi_degrees, -180.0f, 180.0f);
      if (scene.light.type == raypalette::LightType::RectArea) {
        ImGui::TextDisabled("4 deterministic area-light samples.");
      }
      if (light_parameters_changed) {
        const raypalette::Vec3 color = light_color(scene.light);
        const float energy = light_energy(scene.light);
        if (scene.light.type == raypalette::LightType::Point) {
          scene.light =
              raypalette::make_point_light(light_polar, scene.sphere.center, color, energy);
        } else if (scene.light.type == raypalette::LightType::Directional) {
          scene.light = raypalette::make_directional_light(light_polar.theta_degrees,
                                                           light_polar.phi_degrees, color, energy);
        } else {
          scene.light = raypalette::make_rect_area_light(
              light_polar, scene.sphere.center, scene.light.area.normal, scene.light.area.width,
              scene.light.area.height, color, energy);
        }
        request_render();
      }
      if (ImGui::ColorEdit3("Environment color", &scene.environment.color.x,
                            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
        scene.environment.color.x = std::max(0.0f, scene.environment.color.x);
        scene.environment.color.y = std::max(0.0f, scene.environment.color.y);
        scene.environment.color.z = std::max(0.0f, scene.environment.color.z);
        request_render();
      }
      if (ImGui::SliderFloat("Environment intensity", &scene.environment.intensity, 0.0f, 1.0f)) {
        request_render();
      }
    }

    end_settings_panel(light_panel_start);
    const ImVec2 rendering_panel_start = begin_settings_panel();
    // Renerdering settings
    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::SliderInt("Samples per pixel", &settings.samples_per_pixel, 1, 4)) {
        request_render();
      }
      if (ImGui::SliderInt("Light samples", &settings.light_samples_per_frame, 1, 4)) {
        request_render();
      }
      if (ImGui::SliderInt("Target samples", &settings.target_samples_per_pixel, 1, 256)) {
        request_render();
      }
      if (ImGui::SliderInt("Max bounces", &settings.max_bounces, 1, 16)) {
        request_render();
      }
      ImGui::Text("Accumulated samples: %d / %d", renderer.accumulated_samples(),
                  settings.target_samples_per_pixel);
    }
    end_settings_panel(rendering_panel_start);

    // Camera and display controls
    const ImVec2 display_panel_start = begin_settings_panel();
    if (ImGui::CollapsingHeader("Camera / Display", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::SliderFloat("Exposure (EV)", &display_settings.exposure_ev, -4.0f, 4.0f)) {
        clear_palette();
        needs_display_update = true;
      }
      if (ImGui::Checkbox("Reinhard tone mapping", &display_settings.use_reinhard)) {
        clear_palette();
        needs_display_update = true;
      }
      if (ImGui::SliderFloat("Camera distance", &camera_distance, 2.0f, 10.0f)) {
        camera = raypalette::make_default_camera(1.0f, camera_distance);
        request_render();
      }
    }
    end_settings_panel(display_panel_start);
    ImGui::End();

    if (needs_render) {
      image = renderer.render(scene, camera, settings);
      texture.upload(image, display_settings);
      sphere_hsv_points = raypalette::ui::make_sphere_hsv_points(texture, scene, camera, settings);
      needs_render = !renderer.is_accumulation_complete(settings);
      needs_display_update = false;
    } else if (needs_display_update) {
      texture.upload(image, display_settings);
      sphere_hsv_points = raypalette::ui::make_sphere_hsv_points(texture, scene, camera, settings);
      needs_display_update = false;
    }

    ImGui::SetNextWindowSize(ImVec2(layout.preview_panel.width, layout.preview_panel.height),
                             ImGuiCond_FirstUseEver);
    ImGui::Begin("Preview");
    if (texture.id != 0) {
      ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<std::intptr_t>(texture.id)),
                   ImVec2(static_cast<float>(image.width), static_cast<float>(image.height)),
                   ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

      const ImVec2 image_min = ImGui::GetItemRectMin();
      const ImVec2 image_max = ImGui::GetItemRectMax();
      if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float u = (mouse.x - image_min.x) / (image_max.x - image_min.x);
        const float v = (mouse.y - image_min.y) / (image_max.y - image_min.y);
        const int pixel_x = static_cast<int>(u * image.width);
        const int pixel_y = image.height - 1 - static_cast<int>(v * image.height);
        if (pixel_x >= 0 && pixel_x < image.width && pixel_y >= 0 && pixel_y < image.height) {
          const raypalette::Vec3 picked_color =
              texture.display_pixels[pixel_y * image.width + pixel_x];
          if (palette.size() < raypalette::ui::kMaximumPaletteColors) {
            bool already_added = false;
            for (const raypalette::ui::PaletteColor& entry : palette) {
              already_added =
                  already_added || raypalette::ui::same_palette_color(entry.color, picked_color);
            }
            if (!already_added) {
              palette.push_back({picked_color, raypalette::ui::color_to_hex(picked_color), u, v});
              selected_palette_index = static_cast<int>(palette.size()) - 1;
            }
          }
        }
      }

      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
        const raypalette::ui::PaletteColor& entry = palette[index];
        const ImVec2 pin_position(image_min.x + entry.u * (image_max.x - image_min.x),
                                  image_min.y + entry.v * (image_max.y - image_min.y));
        const bool selected = selected_palette_index == index;
        const ImU32 pin_color =
            ImGui::GetColorU32(selected ? ImGuiCol_PlotHistogram : ImGuiCol_Text);
        draw_list->AddCircleFilled(pin_position, selected ? 6.0f : 5.0f, pin_color);
        draw_list->AddCircle(pin_position, selected ? 9.0f : 8.0f,
                             ImGui::GetColorU32(ImGuiCol_WindowBg), 16, 2.0f);
        draw_list->AddText(ImVec2(pin_position.x + 8.0f, pin_position.y - 8.0f), pin_color,
                           std::to_string(index).c_str());
      }
    }
    if (ImGui::BeginTabBar("PreviewTabs")) {
      if (ImGui::BeginTabItem("Palette")) {
        if (ImGui::Button("Clear palette")) {
          palette.clear();
          selected_palette_index = -1;
        }
        for (int index = 0; index < static_cast<int>(palette.size()); ++index) {
          ImGui::PushID(index);
          const raypalette::ui::PaletteColor& entry = palette[index];
          ImGui::Text("%d", index);
          ImGui::SameLine();
          const ImVec4 swatch(entry.color.x, entry.color.y, entry.color.z, 1.0f);
          ImGui::ColorButton("##swatch", swatch, ImGuiColorEditFlags_NoTooltip,
                             ImVec2(32.0f, 24.0f));
          ImGui::SameLine();
          if (ImGui::Selectable(entry.hex.c_str(), selected_palette_index == index,
                                ImGuiSelectableFlags_AllowDoubleClick)) {
            selected_palette_index = index;
          }
          if (selected_palette_index == index && ImGui::GetIO().KeyCtrl &&
              ImGui::IsKeyPressed(ImGuiKey_C)) {
            ImGui::SetClipboardText(entry.hex.c_str());
          }
          ImGui::PopID();
        }
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(layout.hsv_space_panel.width, layout.hsv_space_panel.height),
                             ImGuiCond_FirstUseEver);
    ImGui::Begin("HSV Space");
    const raypalette::Material& sphere_material = scene.materials[raypalette::kSphereMaterialIndex];
    const raypalette::Vec3 display_sphere_color = raypalette::prepare_for_display(
        sphere_material_color(sphere_material), display_settings.exposure_ev,
        display_settings.use_reinhard);
    const raypalette::Vec3 display_light_color = raypalette::prepare_for_display(
        light_color(scene.light), display_settings.exposure_ev, display_settings.use_reinhard);
    const LightEnergyUi energy_ui = light_energy_ui(scene.light.type);
    draw_hsv_space(sphere_hsv_points, display_sphere_color, display_light_color,
                   light_energy(scene.light), energy_ui.maximum, hsv_hue_section, palette,
                   selected_palette_index, hsv_plot_view);
    draw_hsv_hue_section(sphere_hsv_points, hsv_hue_section, display_sphere_color,
                         display_light_color, palette, selected_palette_index);
    ImGui::End();

    ImGui::Render();
    int display_width = 0;
    int display_height = 0;
    glfwGetFramebufferSize(window, &display_width, &display_height);
    glViewport(0, 0, display_width, display_height);
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  texture.destroy();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}