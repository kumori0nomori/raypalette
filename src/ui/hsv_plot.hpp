#pragma once

#include "math/color.hpp"
#include "math/vec3.hpp"
#include "render/camera.hpp"
#include "render/renderer.hpp"
#include "render/scene.hpp"
#include "ui/texture.hpp"

#include <vector>

namespace raypalette::ui {

struct HsvPlotPoint {
  Vec3 display_color;
  Hsv hsv;
  Vec3 cylinder_position;
};

struct HsvHueSection {
  float hue = 0.0f;
  static constexpr float half_width = 1.0f / 120.0f;
};

struct HsvPlotView {
  float yaw = -0.65f;
  float pitch = 0.35f;
  float zoom = 1.0f;
};

Vec3 rotate_hsv_plot_position(const Vec3& position, const HsvPlotView& view);
Vec3 cylinder_position_at_hue(float hue, float saturation, float value);
float hue_distance(float first, float second);

std::vector<HsvPlotPoint> make_sphere_hsv_points(const Texture& texture, const Scene& scene,
                                                 const Camera& camera,
                                                 const RenderSettings& settings);

} // namespace raypalette::ui
