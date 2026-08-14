#include "render/renderer.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace raypalette {
namespace {

bool image_is_finite(const Image& image) {
  for (const Vec3& pixel : image.pixels) {
    if (!std::isfinite(pixel.x) || !std::isfinite(pixel.y) || !std::isfinite(pixel.z)) {
      return false;
    }
  }
  return true;
}

TEST(CpuRenderer, RendersFiniteCanonicalImage) {
  Renderer renderer;
  const Image image =
      renderer.render(make_default_scene(), make_default_camera(1.0f), {8, 8, 0.001f});

  ASSERT_EQ(image.width, 8);
  ASSERT_EQ(image.height, 8);
  ASSERT_EQ(image.pixels.size(), 64U);
  EXPECT_TRUE(image_is_finite(image));
}

TEST(RendererBackend, UsesCpuByDefaultAndCanSelectCpu) {
  Renderer renderer;

  EXPECT_EQ(renderer.backend_type(), RendererBackendType::Cpu);
  EXPECT_EQ(renderer.backend_label(RendererBackendType::Cpu), "CPU");
  EXPECT_TRUE(renderer.is_backend_available(RendererBackendType::Cpu));
  if (renderer.is_backend_available(RendererBackendType::Cuda)) {
    EXPECT_NE(renderer.backend_label(RendererBackendType::Cuda), "GPU (unavailable)");
  } else {
    EXPECT_EQ(renderer.backend_label(RendererBackendType::Cuda), "GPU (unavailable)");
  }
  EXPECT_TRUE(renderer.set_backend(RendererBackendType::Cpu));
  EXPECT_EQ(renderer.backend_type(), RendererBackendType::Cpu);
}

TEST(CpuRenderer, ReturnsEnvironmentForRayMiss) {
  Scene scene = make_default_scene();
  scene.environment.color = {0.25f, 0.5f, 0.75f};
  scene.environment.intensity = 1.0f;
  const Camera miss_camera{
      {0.0f, 1.0f, 5.0f}, {0.0f, 1.0f, 6.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
  Renderer renderer;
  const Image image = renderer.render(scene, miss_camera, {1, 1, 0.001f});

  EXPECT_NEAR(image.pixels.front().x, 0.25f, 1.0e-6f);
  EXPECT_NEAR(image.pixels.front().y, 0.5f, 1.0e-6f);
  EXPECT_NEAR(image.pixels.front().z, 0.75f, 1.0e-6f);
}

TEST(CpuRenderer, AccumulatesProgressiveFrames) {
  Renderer renderer;
  RenderSettings settings{4, 4, 0.001f, 2, 4, 4};
  const Scene scene = make_default_scene();
  const Camera camera = make_default_camera(1.0f);

  renderer.render(scene, camera, settings);
  EXPECT_EQ(renderer.accumulated_samples(), 2);
  EXPECT_FALSE(renderer.is_accumulation_complete(settings));
  renderer.render(scene, camera, settings);
  EXPECT_EQ(renderer.accumulated_samples(), 4);
  EXPECT_TRUE(renderer.is_accumulation_complete(settings));
}

TEST(CpuRenderer, RendersEmissionWithoutDirectLight) {
  Scene scene = make_default_scene();
  Material& sphere_material = scene.materials[kSphereMaterialIndex];
  sphere_material.type = MaterialType::Emissive;
  sphere_material.base_color = {};
  sphere_material.emission_color = {0.2f, 0.4f, 0.8f};
  sphere_material.emission_strength = 2.0f;
  scene.environment.intensity = 0.0f;
  scene.light.point.radiant_intensity = 0.0f;

  Renderer renderer;
  const Image image = renderer.render(scene, make_default_camera(1.0f), {8, 8, 0.001f, 1, 1});
  const Vec3& center_pixel = image.pixels[4 * image.width + 4];

  EXPECT_NEAR(center_pixel.x, 0.4f, 1.0e-5f);
  EXPECT_NEAR(center_pixel.y, 0.8f, 1.0e-5f);
  EXPECT_NEAR(center_pixel.z, 1.6f, 1.0e-5f);
}

} // namespace
} // namespace raypalette