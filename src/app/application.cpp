#include "render/renderer.hpp"
#include "math/color.hpp"

#include <GLFW/glfw3.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include <algorithm>
#include <cstdint>
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
  WindowSize controls_panel{360.0f, 620.0f};
  WindowSize preview_panel{600.0f, 600.0f};
};

constexpr GuiLayout kDefaultGuiLayout{};

struct DisplaySettings {
  float exposure_ev = 0.0f;
  bool use_reinhard = true;
};

struct Texture {
  GLuint id = 0;
  int width = 0;
  int height = 0;
  std::vector<raypalette::Vec3> display_pixels;

  void create(int new_width, int new_height) {
    width = new_width;
    height = new_height;
    if (id == 0) {
      glGenTextures(1, &id);
    }
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT,
                 nullptr);
  }

  void upload(const raypalette::Image &image,
              const DisplaySettings &display_settings) {
    if (image.width != width || image.height != height) {
      create(image.width, image.height);
    }
    // Convert linear color to sRGB for display.
    display_pixels.resize(image.pixels.size());
    for (std::size_t index = 0; index < image.pixels.size(); ++index) {
      display_pixels[index] = raypalette::prepare_for_display(
          image.pixels[index], display_settings.exposure_ev,
          display_settings.use_reinhard);
    }
    glBindTexture(GL_TEXTURE_2D, id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image.width, image.height, GL_RGB,
                    GL_FLOAT, display_pixels.data());
  }

  void destroy() {
    if (id != 0) {
      glDeleteTextures(1, &id);
      id = 0;
    }
  }
};

raypalette::Vec3 &light_color(raypalette::Light &light) {
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

float &light_energy(raypalette::Light &light) {
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
  const char *label;
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

struct MaterialUi {
  const char *const *labels;
  int label_count;
};

constexpr const char *kMaterialLabels[] = {
  "Diffuse", 
  "Metal",
  "Glass",
  "Emissive"
};
constexpr MaterialUi kSphereMaterialUi{
    kMaterialLabels, static_cast<int>(std::size(kMaterialLabels))};

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
  const GuiLayout &layout = kDefaultGuiLayout;
  GLFWwindow *window = glfwCreateWindow(
    static_cast<int>(layout.main_window.width),
    static_cast<int>(layout.main_window.height),
    "RayPalette", nullptr, nullptr);
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
  raypalette::RenderSettings settings{
    /*width=*/512, 
    /*height=*/512, 
    /*minimum_distance=*/0.001f, 
    /*samples_per_pixel=*/4, 
    /*light_samples_per_frame=*/4, 
    /*target_samples_per_pixel=*/64, 
    /*max_bounces=*/8
  };
  DisplaySettings display_settings;
  float camera_distance = 5.0f;
  raypalette::Camera camera = raypalette::make_default_camera(
    1.0f, camera_distance);
  raypalette::Renderer renderer;
  Texture texture;
  raypalette::Image image;
  bool needs_render = true;
  bool needs_display_update = true;
  auto request_render = [&]() {
    renderer.reset_accumulation();
    needs_render = true;
  };
  float settings_panel_right = 0.0f;
  auto begin_settings_panel = [&]() {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    settings_panel_right = start.x + ImGui::GetContentRegionAvail().x;
    ImGui::BeginGroup();
    return start;
  };
  auto end_settings_panel = [&](const ImVec2 &start) {
    ImGui::EndGroup();
    const float panel_bottom = ImGui::GetItemRectMax().y;
    ImGui::GetWindowDrawList()->AddRect(
        ImVec2(start.x - 4.0f, start.y - 3.0f),
      ImVec2(settings_panel_right + 4.0f, panel_bottom + 3.0f),
        ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
  };

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(
      ImVec2(layout.controls_panel.width, layout.controls_panel.height),
      ImGuiCond_FirstUseEver);
    ImGui::Begin("RayPalette Controls");
    ImGui::Text("Deterministic CUDA preview");
    if (ImGui::Button("Reset scene")) {
      scene = raypalette::make_default_scene();
      light_polar = {4.0f, 35.0f, 45.0f};
      light_type_index = static_cast<int>(scene.light.type);
      camera_distance = 5.0f;
      camera = raypalette::make_default_camera(1.0f, camera_distance);
      request_render();
    }

    // Sphere and materials controls
    const ImVec2 sphere_panel_start = begin_settings_panel();
    if (ImGui::CollapsingHeader("Sphere & Materials",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::ColorEdit3(
            "Sphere color",
            &scene.materials[raypalette::kSphereMaterialIndex].base_color.x,
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
        request_render();
      }
      int sphere_material_index = material_type_index(
        scene.materials[raypalette::kSphereMaterialIndex].type);
      if (ImGui::Combo("Sphere material", &sphere_material_index,
                      kSphereMaterialUi.labels,
                      kSphereMaterialUi.label_count)) {
        scene.materials[raypalette::kSphereMaterialIndex].type =
          material_type_from_index(sphere_material_index);
        request_render();
      }
      if (scene.materials[raypalette::kSphereMaterialIndex].type ==
            raypalette::MaterialType::Metal) {
        if (ImGui::SliderFloat(
              "Metal roughness",
              &scene.materials[raypalette::kSphereMaterialIndex].roughness,
              0.0f, 1.0f)) {
          request_render();
        }
        ImGui::TextDisabled("Perfect mirror reflection; roughness is reserved for GGX.");
      }
      if (scene.materials[raypalette::kSphereMaterialIndex].type ==
          raypalette::MaterialType::Dielectric) {
        if (ImGui::SliderFloat(
              "Glass IOR",
              &scene.materials[raypalette::kSphereMaterialIndex].index_of_refraction,
              1.01f, 3.0f)) {
          request_render();
        }
        if (ImGui::ColorEdit3(
              "Glass transmission",
              &scene.materials[raypalette::kSphereMaterialIndex].transmission_color.x,
              ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
          scene.materials[raypalette::kSphereMaterialIndex].transmission_color.x =
            std::max(0.0f, scene.materials[raypalette::kSphereMaterialIndex]
                    .transmission_color.x);
          scene.materials[raypalette::kSphereMaterialIndex].transmission_color.y =
            std::max(0.0f, scene.materials[raypalette::kSphereMaterialIndex]
                    .transmission_color.y);
          scene.materials[raypalette::kSphereMaterialIndex].transmission_color.z =
            std::max(0.0f, scene.materials[raypalette::kSphereMaterialIndex]
                    .transmission_color.z);
          request_render();
        }
        if (ImGui::SliderFloat(
              "Glass absorption density",
              &scene.materials[raypalette::kSphereMaterialIndex].absorption_density,
              0.0f, 5.0f)) {
          request_render();
        }
        ImGui::TextDisabled("Clear glass: Fresnel reflection and refraction.");
      }
      if (ImGui::ColorEdit3(
            "Floor color",
            &scene.materials[raypalette::kFloorMaterialIndex].base_color.x,
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
        request_render();
      }
      if (scene.materials[raypalette::kSphereMaterialIndex].type ==
          raypalette::MaterialType::Emissive) {
        if (ImGui::ColorEdit3(
              "Sphere emission",
              &scene.materials[raypalette::kSphereMaterialIndex].emission_color.x,
              ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs)) {
          scene.materials[raypalette::kSphereMaterialIndex].emission_color.x =
            std::max(0.0f, scene.materials[raypalette::kSphereMaterialIndex]
                      .emission_color.x);
          scene.materials[raypalette::kSphereMaterialIndex].emission_color.y =
            std::max(0.0f, scene.materials[raypalette::kSphereMaterialIndex]
                      .emission_color.y);
          scene.materials[raypalette::kSphereMaterialIndex].emission_color.z =
            std::max(0.0f, scene.materials[raypalette::kSphereMaterialIndex]
                      .emission_color.z);
          request_render();
        }
        if (ImGui::SliderFloat(
              "Sphere emission strength",
              &scene.materials[raypalette::kSphereMaterialIndex].emission_strength,
              0.0f, 10.0f)) {
          request_render();
        }
      }
    }

    end_settings_panel(sphere_panel_start);

    // Light controls
    const ImVec2 light_panel_start = begin_settings_panel();
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
      const char *light_types[] = {"Point", "Rectangular area", "Directional (sun)"};
      if (ImGui::Combo("Light type", &light_type_index, light_types,
                      IM_ARRAYSIZE(light_types))) {
        const raypalette::LightType type =
          static_cast<raypalette::LightType>(light_type_index);
        const raypalette::Vec3 color = light_color(scene.light);
        const float energy = light_energy(scene.light);
        if (type == raypalette::LightType::Point) {
          scene.light = raypalette::make_point_light(
            light_polar, scene.sphere.center, color, energy);
        } else if (type == raypalette::LightType::Directional) {
          scene.light = raypalette::make_directional_light(
            light_polar.theta_degrees, light_polar.phi_degrees,
            color, energy);
        } else {
          scene.light = raypalette::make_rect_area_light(
            light_polar, scene.sphere.center, {0.0f, -1.0f, 0.0f},
            scene.light.area.width, scene.light.area.height, color, energy);
        }
        request_render();
      }

      const LightEnergyUi energy_ui = light_energy_ui(scene.light.type);
      if (ImGui::SliderFloat(energy_ui.label, &light_energy(scene.light),
                            energy_ui.minimum, energy_ui.maximum)) {
        request_render();
      }
      float light_color_values[3] = {light_color(scene.light).x,
                                    light_color(scene.light).y,
                                    light_color(scene.light).z};
      if (ImGui::ColorEdit3("Light color", light_color_values,
                ImGuiColorEditFlags_NoInputs)) {
        light_color(scene.light) = {
          std::max(0.0f, light_color_values[0]),
          std::max(0.0f, light_color_values[1]),
          std::max(0.0f, light_color_values[2])};
        request_render();
      }
      bool light_parameters_changed = false;
      if (scene.light.type != raypalette::LightType::Directional) {
        light_parameters_changed |=
          ImGui::SliderFloat("Light radius", &light_polar.radius, 0.1f, 20.0f);
      } else {
        ImGui::TextDisabled("Sun light has no radius or distance falloff.");
      }
      light_parameters_changed |= ImGui::SliderFloat(
        "Light theta", &light_polar.theta_degrees, 0.0f, 90.0f);
      light_parameters_changed |= ImGui::SliderFloat(
        "Light phi", &light_polar.phi_degrees, -180.0f, 180.0f);
      if (scene.light.type == raypalette::LightType::RectArea) {
        light_parameters_changed |=
          ImGui::SliderFloat("Area width", &scene.light.area.width, 0.1f, 20.0f);
        light_parameters_changed |=
          ImGui::SliderFloat("Area height", &scene.light.area.height, 0.1f, 20.0f);
        ImGui::TextDisabled("4 deterministic area-light samples.");
      }
      if (light_parameters_changed) {
        const raypalette::Vec3 color = light_color(scene.light);
        const float energy = light_energy(scene.light);
        if (scene.light.type == raypalette::LightType::Point) {
          scene.light = raypalette::make_point_light(
            light_polar, scene.sphere.center, color, energy);
        } else if (scene.light.type == raypalette::LightType::Directional) {
          scene.light = raypalette::make_directional_light(
            light_polar.theta_degrees, light_polar.phi_degrees,
            color, energy);
        } else {
          scene.light = raypalette::make_rect_area_light(
            light_polar, scene.sphere.center, scene.light.area.normal,
            scene.light.area.width, scene.light.area.height, color, energy);
        }
        request_render();
      }
      if (ImGui::ColorEdit3("Environment color", &scene.environment.color.x,
                            ImGuiColorEditFlags_Float |
                              ImGuiColorEditFlags_NoInputs)) {
        scene.environment.color.x = std::max(0.0f, scene.environment.color.x);
        scene.environment.color.y = std::max(0.0f, scene.environment.color.y);
        scene.environment.color.z = std::max(0.0f, scene.environment.color.z);
        request_render();
      }
      if (ImGui::SliderFloat("Environment intensity",
                            &scene.environment.intensity, 0.0f, 1.0f)) {
        request_render();
      }
    }

    end_settings_panel(light_panel_start);
    const ImVec2 rendering_panel_start = begin_settings_panel();
    // Renerdering settings
    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::SliderInt("Samples per pixel", &settings.samples_per_pixel, 1,
                           4)) {
        request_render();
      }
      if (ImGui::SliderInt("Light samples", &settings.light_samples_per_frame,
                           1, 4)) {
        request_render();
      }
      if (ImGui::SliderInt("Target samples",
                           &settings.target_samples_per_pixel, 1, 256)) {
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
    if (ImGui::CollapsingHeader("Camera / Display",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::ColorEdit3("Background", &scene.background_color.x,
                            ImGuiColorEditFlags_Float |
                              ImGuiColorEditFlags_NoInputs)) {
        scene.background_color.x = std::max(0.0f, scene.background_color.x);
        scene.background_color.y = std::max(0.0f, scene.background_color.y);
        scene.background_color.z = std::max(0.0f, scene.background_color.z);
        request_render();
      }
      if (ImGui::SliderFloat("Exposure (EV)", &display_settings.exposure_ev,
                            -4.0f, 4.0f)) {
        needs_display_update = true;
      }
      if (ImGui::Checkbox("Reinhard tone mapping",
                          &display_settings.use_reinhard)) {
        needs_display_update = true;
      }
      if (ImGui::SliderFloat("Camera distance", &camera_distance, 2.0f,
                             10.0f)) {
        camera = raypalette::make_default_camera(1.0f, camera_distance);
        request_render();
      }
    }
    end_settings_panel(display_panel_start);
    ImGui::End();

    if (needs_render) {
      image = renderer.render(scene, camera, settings);
      texture.upload(image, display_settings);
      needs_render = !renderer.is_accumulation_complete(settings);
      needs_display_update = false;
    } else if (needs_display_update) {
      texture.upload(image, display_settings);
      needs_display_update = false;
    }

    ImGui::SetNextWindowSize(
      ImVec2(layout.preview_panel.width, layout.preview_panel.height),
      ImGuiCond_FirstUseEver);
    ImGui::Begin("Preview");
    if (texture.id != 0) {
      ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<std::intptr_t>(texture.id)),
                   ImVec2(static_cast<float>(image.width), static_cast<float>(image.height)),
                   ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    }
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