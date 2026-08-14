#include "ui/controls.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace raypalette::ui {

namespace {

constexpr const char* kMaterialLabels[] = {"Diffuse", "Metal", "Glass", "Emissive"};
constexpr const char* kAreaLightSampleLabels[] = {"4 (2x2)", "9 (3x3)", "16 (4x4)"};

int area_light_sample_index(int sample_count) {
  switch (sample_count) {
  case 9:
    return 1;
  case 16:
    return 2;
  default:
    return 0;
  }
}

int area_light_sample_count(int index) {
  switch (index) {
  case 1:
    return 9;
  case 2:
    return 16;
  default:
    return 4;
  }
}

} // namespace

Vec3& light_color(Light& light) {
  switch (light.type) {
  case LightType::Point:
    return light.point.color;
  case LightType::Directional:
    return light.directional.color;
  case LightType::RectArea:
    return light.area.color;
  }
  return light.point.color;
}

float& light_energy(Light& light) {
  switch (light.type) {
  case LightType::Point:
    return light.point.radiant_intensity;
  case LightType::Directional:
    return light.directional.irradiance;
  case LightType::RectArea:
    return light.area.radiance;
  }
  return light.point.radiant_intensity;
}

LightEnergyUi light_energy_ui(LightType type) {
  switch (type) {
  case LightType::Point:
    return {"Radiant intensity", 0.0f, 500.0f};
  case LightType::Directional:
    return {"Sun irradiance", 0.0f, 10.0f};
  case LightType::RectArea:
    return {"Area radiance", 0.0f, 50.0f};
  }
  return {"Light energy", 0.0f, 1.0f};
}

int material_type_index(MaterialType type) {
  switch (type) {
  case MaterialType::Diffuse:
    return 0;
  case MaterialType::Metal:
    return 1;
  case MaterialType::Dielectric:
    return 2;
  case MaterialType::Emissive:
    return 3;
  }
  return 0;
}

MaterialType material_type_from_index(int index) {
  switch (index) {
  case 1:
    return MaterialType::Metal;
  case 2:
    return MaterialType::Dielectric;
  case 3:
    return MaterialType::Emissive;
  default:
    return MaterialType::Diffuse;
  }
}

Vec3 sphere_material_color(const Material& material) {
  switch (material.type) {
  case MaterialType::Dielectric:
    return material.transmission_color;
  case MaterialType::Emissive:
    return material.emission_color;
  case MaterialType::Diffuse:
  case MaterialType::Metal:
    return material.base_color;
  }
  return material.base_color;
}

void draw_controls(ControlsContext& context) {
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

  ImGui::SetNextWindowSize(ImVec2(360.0f, 750.0f), ImGuiCond_FirstUseEver);
  ImGui::Begin("Raypalette Controls");
  if (ImGui::Button("Reset scene")) {
    context.scene = make_default_scene();
    context.light_polar = {4.0f, 35.0f, 45.0f};
    context.light_type_index = static_cast<int>(context.scene.light.type);
    context.camera_distance = 5.0f;
    context.camera = make_default_camera(1.0f, context.camera_distance);
    context.clear_palette();
    context.request_render();
  }

  const ImVec2 sphere_panel_start = begin_settings_panel();
  if (ImGui::CollapsingHeader("Sphere & Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
    Material& sphere_material = context.scene.materials[kSphereMaterialIndex];
    int sphere_material_index = material_type_index(sphere_material.type);
    if (ImGui::Combo("Sphere material", &sphere_material_index, kMaterialLabels,
                     static_cast<int>(std::size(kMaterialLabels)))) {
      const MaterialType previous_type = sphere_material.type;
      const MaterialType next_type = material_type_from_index(sphere_material_index);
      if (next_type == MaterialType::Dielectric && previous_type != MaterialType::Dielectric) {
        sphere_material.transmission_color = previous_type == MaterialType::Emissive
                                                 ? sphere_material.emission_color
                                                 : sphere_material.base_color;
        sphere_material.base_color = {1.0f, 1.0f, 1.0f};
      } else if (next_type == MaterialType::Emissive && previous_type != MaterialType::Emissive) {
        sphere_material.emission_color = previous_type == MaterialType::Dielectric
                                             ? sphere_material.transmission_color
                                             : sphere_material.base_color;
        sphere_material.base_color = {1.0f, 1.0f, 1.0f};
      } else if (previous_type == MaterialType::Dielectric &&
                 next_type != MaterialType::Dielectric) {
        sphere_material.base_color = sphere_material.transmission_color;
      } else if (previous_type == MaterialType::Emissive && next_type != MaterialType::Emissive) {
        sphere_material.base_color = sphere_material.emission_color;
      }
      sphere_material.type = next_type;
      context.request_render();
    }
    if (sphere_material.type == MaterialType::Dielectric) {
      if (ImGui::ColorEdit3("Glass tint", &sphere_material.transmission_color.x,
                            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
        sphere_material.transmission_color.x = std::max(0.0f, sphere_material.transmission_color.x);
        sphere_material.transmission_color.y = std::max(0.0f, sphere_material.transmission_color.y);
        sphere_material.transmission_color.z = std::max(0.0f, sphere_material.transmission_color.z);
        context.request_render();
      }
    } else if (sphere_material.type == MaterialType::Emissive) {
      const bool emission_color_changed =
          ImGui::ColorEdit3("Emission color", &sphere_material.emission_color.x,
                            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
      if (emission_color_changed) {
        sphere_material.emission_color.x = std::max(0.0f, sphere_material.emission_color.x);
        sphere_material.emission_color.y = std::max(0.0f, sphere_material.emission_color.y);
        sphere_material.emission_color.z = std::max(0.0f, sphere_material.emission_color.z);
        context.request_render();
      }
    } else if (ImGui::ColorEdit3("Color", &sphere_material.base_color.x,
                                 ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
      context.request_render();
    }
    if (sphere_material.type == MaterialType::Metal &&
        ImGui::SliderFloat("Metal roughness", &sphere_material.roughness, 0.0f, 1.0f)) {
      context.request_render();
    }
    if (sphere_material.type == MaterialType::Dielectric) {
      if (ImGui::SliderFloat("Glass absorption density", &sphere_material.absorption_density, 0.0f,
                             5.0f)) {
        context.request_render();
      }
      constexpr float kMinimumGlassIor = 1.01f;
      constexpr float kMaximumGlassIor = 3.0f;
      const float ior_log_denominator = std::log(kMaximumGlassIor);
      float ior_slider = std::log(std::max(kMinimumGlassIor, sphere_material.index_of_refraction)) /
                         ior_log_denominator;
      const float minimum_ior_slider = std::log(kMinimumGlassIor) / ior_log_denominator;
      if (ImGui::SliderFloat("Glass IOR##slider", &ior_slider, minimum_ior_slider, 1.0f, "")) {
        sphere_material.index_of_refraction =
            1.0f + (std::exp(ior_slider * ior_log_denominator) - 1.0f);
        context.request_render();
      }
      ImGui::SameLine();
      ImGui::Text("IOR %.3f", sphere_material.index_of_refraction);
    }
    if (sphere_material.type == MaterialType::Emissive &&
        ImGui::SliderFloat("Emission strength", &sphere_material.emission_strength, 0.0f, 10.0f)) {
      context.request_render();
    }
    if (ImGui::ColorEdit3("Floor color", &context.scene.materials[kFloorMaterialIndex].base_color.x,
                          ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
      context.request_render();
    }
  }
  end_settings_panel(sphere_panel_start);

  const ImVec2 light_panel_start = begin_settings_panel();
  if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
    const char* light_types[] = {"Point", "Rectangular area", "Directional (sun)"};
    if (ImGui::Combo("Light type", &context.light_type_index, light_types,
                     IM_ARRAYSIZE(light_types))) {
      const LightType type = static_cast<LightType>(context.light_type_index);
      const Vec3 color = light_color(context.scene.light);
      const float energy = light_energy(context.scene.light);
      if (type == LightType::Point) {
        context.scene.light =
            make_point_light(context.light_polar, context.scene.sphere.center, color, energy);
      } else if (type == LightType::Directional) {
        context.scene.light = make_directional_light(
            context.light_polar.theta_degrees, context.light_polar.phi_degrees, color, energy);
      } else {
        context.scene.light = make_rect_area_light(
            context.light_polar, context.scene.sphere.center, {0.0f, -1.0f, 0.0f},
            context.scene.light.area.width, context.scene.light.area.height, color, energy);
      }
      context.request_render();
    }
    float light_color_values[3] = {light_color(context.scene.light).x,
                                   light_color(context.scene.light).y,
                                   light_color(context.scene.light).z};
    if (ImGui::ColorEdit3("Light color", light_color_values, ImGuiColorEditFlags_NoInputs)) {
      light_color(context.scene.light) = {std::max(0.0f, light_color_values[0]),
                                          std::max(0.0f, light_color_values[1]),
                                          std::max(0.0f, light_color_values[2])};
      context.request_render();
    }
    const LightEnergyUi energy_ui = light_energy_ui(context.scene.light.type);
    if (ImGui::SliderFloat(energy_ui.label, &light_energy(context.scene.light), energy_ui.minimum,
                           energy_ui.maximum)) {
      context.request_render();
    }
    if (context.scene.light.type == LightType::RectArea) {
      float area_size = 0.5f * (context.scene.light.area.width + context.scene.light.area.height);
      if (ImGui::SliderFloat("Area size", &area_size, 0.1f, 20.0f)) {
        context.scene.light.area.width = area_size;
        context.scene.light.area.height = area_size;
        context.request_render();
      }
      int area_light_sample_index_value =
          area_light_sample_index(context.settings.light_samples_per_frame);
      if (ImGui::Combo("Area light samples", &area_light_sample_index_value, kAreaLightSampleLabels,
                       IM_ARRAYSIZE(kAreaLightSampleLabels))) {
        context.settings.light_samples_per_frame =
            area_light_sample_count(area_light_sample_index_value);
        context.request_render();
      }
    }
    bool light_parameters_changed = false;
    if (context.scene.light.type != LightType::Directional) {
      light_parameters_changed |=
          ImGui::SliderFloat("Light radius", &context.light_polar.radius, 0.1f, 20.0f);
    } else {
      ImGui::TextDisabled("Sun light has no radius or distance falloff.");
    }
    light_parameters_changed |=
        ImGui::SliderFloat("Light theta", &context.light_polar.theta_degrees, 0.0f, 90.0f);
    light_parameters_changed |=
        ImGui::SliderFloat("Light phi", &context.light_polar.phi_degrees, -180.0f, 180.0f);
    if (light_parameters_changed) {
      const Vec3 color = light_color(context.scene.light);
      const float energy = light_energy(context.scene.light);
      if (context.scene.light.type == LightType::Point) {
        context.scene.light =
            make_point_light(context.light_polar, context.scene.sphere.center, color, energy);
      } else if (context.scene.light.type == LightType::Directional) {
        context.scene.light = make_directional_light(
            context.light_polar.theta_degrees, context.light_polar.phi_degrees, color, energy);
      } else {
        context.scene.light = make_rect_area_light(
            context.light_polar, context.scene.sphere.center, context.scene.light.area.normal,
            context.scene.light.area.width, context.scene.light.area.height, color, energy);
      }
      context.request_render();
    }
    if (ImGui::ColorEdit3("Environment color", &context.scene.environment.color.x,
                          ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
      context.scene.environment.color.x = std::max(0.0f, context.scene.environment.color.x);
      context.scene.environment.color.y = std::max(0.0f, context.scene.environment.color.y);
      context.scene.environment.color.z = std::max(0.0f, context.scene.environment.color.z);
      context.request_render();
    }
    if (ImGui::SliderFloat("Environment intensity", &context.scene.environment.intensity, 0.0f,
                           1.0f)) {
      context.request_render();
    }
  }
  end_settings_panel(light_panel_start);

  const ImVec2 rendering_panel_start = begin_settings_panel();
  if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
    const RendererBackendType current_backend = context.renderer.backend_type();
    const bool cuda_available = context.renderer.is_backend_available(RendererBackendType::Cuda);
    const std::string gpu_label = context.renderer.backend_label(RendererBackendType::Cuda);
    if (ImGui::RadioButton("CPU##renderer_backend", current_backend == RendererBackendType::Cpu)) {
      if (context.renderer.set_backend(RendererBackendType::Cpu)) {
        context.request_render();
      }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!cuda_available);
    if (ImGui::RadioButton((gpu_label + "##renderer_backend").c_str(),
                           current_backend == RendererBackendType::Cuda)) {
      if (context.renderer.set_backend(RendererBackendType::Cuda)) {
        context.request_render();
      }
    }
    ImGui::EndDisabled();
    if (ImGui::SliderInt("Samples per pixel", &context.settings.samples_per_pixel, 1, 16)) {
      context.request_render();
    }
    if (ImGui::SliderInt("Target samples", &context.settings.target_samples_per_pixel, 1, 256)) {
      context.request_render();
    }
    if (ImGui::SliderInt("Max bounces", &context.settings.max_bounces, 1, 16)) {
      context.request_render();
    }
    ImGui::Text("Accumulated samples: %d / %d", context.renderer.accumulated_samples(),
                context.settings.target_samples_per_pixel);
  }
  end_settings_panel(rendering_panel_start);

  const ImVec2 display_panel_start = begin_settings_panel();
  if (ImGui::CollapsingHeader("Camera / Display", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::SliderFloat("Exposure (EV)", &context.gui_state.display_settings.exposure_ev, -4.0f,
                           4.0f)) {
      context.clear_palette();
      context.gui_state.needs_display_update = true;
    }
    if (ImGui::Checkbox("Reinhard tone mapping",
                        &context.gui_state.display_settings.use_reinhard)) {
      context.clear_palette();
      context.gui_state.needs_display_update = true;
    }
    if (ImGui::SliderFloat("Camera distance", &context.camera_distance, 2.0f, 10.0f)) {
      context.camera = make_default_camera(1.0f, context.camera_distance);
      context.request_render();
    }
  }
  end_settings_panel(display_panel_start);
  ImGui::End();
}

} // namespace raypalette::ui
