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
    raypalette::ui::draw_hsv_space(sphere_hsv_points, display_sphere_color, display_light_color,
                                   light_energy(scene.light), energy_ui.maximum, hsv_hue_section,
                                   palette, selected_palette_index, hsv_plot_view);
    raypalette::ui::draw_hsv_hue_section(sphere_hsv_points, hsv_hue_section, display_sphere_color,
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