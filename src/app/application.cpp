#include "render/renderer.hpp"

#include <GLFW/glfw3.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

struct Texture {
  GLuint id = 0;
  int width = 0;
  int height = 0;

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

  void upload(const raypalette::Image &image) {
    if (image.width != width || image.height != height) {
      create(image.width, image.height);
    }
    glBindTexture(GL_TEXTURE_2D, id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image.width, image.height, GL_RGB,
                    GL_FLOAT, image.pixels.data());
  }

  void destroy() {
    if (id != 0) {
      glDeleteTextures(1, &id);
      id = 0;
    }
  }
};

} // namespace

int main() {
  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow *window = glfwCreateWindow(1100, 760, "RayPalette", nullptr, nullptr);
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
  raypalette::RenderSettings settings{512, 512, 0.001f};
  raypalette::Camera camera = raypalette::make_default_camera(1.0f);
  raypalette::Renderer renderer;
  Texture texture;
  raypalette::Image image;
  bool needs_render = true;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("RayPalette Controls");
    ImGui::Text("Deterministic CUDA preview");
    if (ImGui::Button("Reset scene")) {
      scene = raypalette::make_default_scene();
      needs_render = true;
    }
    if (ImGui::ColorEdit3(
            "Sphere color",
            &scene.materials[raypalette::kSphereMaterialIndex].base_color.x,
            ImGuiColorEditFlags_Float)) {
      needs_render = true;
    }
    if (ImGui::ColorEdit3(
            "Floor color",
            &scene.materials[raypalette::kFloorMaterialIndex].base_color.x,
            ImGuiColorEditFlags_Float)) {
      needs_render = true;
    }
    if (ImGui::ColorEdit3("Background", &scene.background_color.x,
                          ImGuiColorEditFlags_Float)) {
      scene.background_color.x = std::max(0.0f, scene.background_color.x);
      scene.background_color.y = std::max(0.0f, scene.background_color.y);
      scene.background_color.z = std::max(0.0f, scene.background_color.z);
      needs_render = true;
    }
    if (ImGui::SliderFloat("Light intensity", &scene.light.intensity, 0.0f, 500.0f)) {
      needs_render = true;
    }
    float light_color[3] = {scene.light.color.x, scene.light.color.y, scene.light.color.z};
    if (ImGui::ColorEdit3("Light color", light_color)) {
      scene.light.color = {std::max(0.0f, light_color[0]), std::max(0.0f, light_color[1]),
                           std::max(0.0f, light_color[2])};
      needs_render = true;
    }
    ImGui::End();

    if (needs_render) {
      image = renderer.render(scene, camera, settings);
      texture.upload(image);
      needs_render = false;
    }

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