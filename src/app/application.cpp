#include "math/color.hpp"
#include "render/renderer.hpp"
#include "ui/controls.hpp"
#include "ui/hsv_plot.hpp"
#include "ui/hsv_plot_gui.hpp"
#include "ui/palette.hpp"
#include "ui/texture.hpp"

#include <GLFW/glfw3.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include <cstdint>
#include <cstdio>
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
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    raypalette::ui::ControlsContext controls_context{
        scene,        light_polar,          light_type_index, settings,
        camera,       camera_distance,      renderer,         display_settings,
        needs_render, needs_display_update, clear_palette,    request_render};
    raypalette::ui::draw_controls(controls_context);

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
        raypalette::ui::sphere_material_color(sphere_material), display_settings.exposure_ev,
        display_settings.use_reinhard);
    const raypalette::Vec3 display_light_color = raypalette::prepare_for_display(
        raypalette::ui::light_color(scene.light), display_settings.exposure_ev,
        display_settings.use_reinhard);
    const raypalette::ui::LightEnergyUi energy_ui =
        raypalette::ui::light_energy_ui(scene.light.type);
    raypalette::ui::draw_hsv_space(sphere_hsv_points, display_sphere_color, display_light_color,
                                   raypalette::ui::light_energy(scene.light), energy_ui.maximum,
                                   hsv_hue_section, palette, selected_palette_index, hsv_plot_view);
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