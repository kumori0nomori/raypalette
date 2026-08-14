#include "math/color.hpp"
#include "render/renderer.hpp"
#include "ui/controls.hpp"
#include "ui/hsv_plot.hpp"
#include "ui/hsv_plot_gui.hpp"
#include "ui/palette.hpp"
#include "ui/preview.hpp"
#include "ui/texture.hpp"

#include <GLFW/glfw3.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include <cstdio>
#include <stdexcept>
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

    raypalette::ui::PreviewContext preview_context{
        image,
        texture,
        palette,
        selected_palette_index,
        {layout.preview_panel.width, layout.preview_panel.height}};
    raypalette::ui::draw_preview(preview_context);

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