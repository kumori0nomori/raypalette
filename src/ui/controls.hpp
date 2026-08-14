#pragma once

#include "render/camera.hpp"
#include "render/light.hpp"
#include "render/material.hpp"
#include "render/renderer.hpp"
#include "render/scene.hpp"
#include "ui/texture.hpp"

#include <functional>

namespace raypalette::ui {

struct ControlsContext {
  Scene& scene;
  PolarCoordinates& light_polar;
  int& light_type_index;
  RenderSettings& settings;
  Camera& camera;
  float& camera_distance;
  Renderer& renderer;
  DisplaySettings& display_settings;
  bool& needs_render;
  bool& needs_display_update;
  std::function<void()> clear_palette;
  std::function<void()> request_render;
};

struct LightEnergyUi {
  const char* label;
  float minimum;
  float maximum;
};

Vec3& light_color(Light& light);
float& light_energy(Light& light);
LightEnergyUi light_energy_ui(LightType type);

int material_type_index(MaterialType type);
MaterialType material_type_from_index(int index);
Vec3 sphere_material_color(const Material& material);

void draw_controls(ControlsContext& context);

} // namespace raypalette::ui
