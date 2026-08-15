#include "render/renderer.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>

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

float image_difference(const Image& first, const Image& second) {
  float difference = 0.0f;
  for (std::size_t pixel_index = 0; pixel_index < first.pixels.size(); ++pixel_index) {
    difference += std::fabs(first.pixels[pixel_index].x - second.pixels[pixel_index].x);
    difference += std::fabs(first.pixels[pixel_index].y - second.pixels[pixel_index].y);
    difference += std::fabs(first.pixels[pixel_index].z - second.pixels[pixel_index].z);
  }
  return difference;
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

TEST(CpuRenderer, RendersFiniteSurfacePresets) {
  const MaterialPreset presets[] = {
      MaterialPreset::Custom, MaterialPreset::Matte, MaterialPreset::Glossy, MaterialPreset::Metal,
      MaterialPreset::Cloth,  MaterialPreset::Skin,  MaterialPreset::Hair};
  Renderer renderer;
  for (const MaterialPreset preset : presets) {
    Scene scene = make_default_scene();
    apply_material_preset(scene.materials[kSphereMaterialIndex], preset);
    const Image image = renderer.render(scene, make_default_camera(1.0f), {4, 4, 0.001f, 1, 1});
    EXPECT_TRUE(image_is_finite(image));
  }
}

TEST(CpuRenderer, SurfacePresetsChangeDirectLighting) {
  Scene matte_scene = make_default_scene();
  apply_material_preset(matte_scene.materials[kSphereMaterialIndex], MaterialPreset::Matte);
  Scene glossy_scene = matte_scene;
  apply_material_preset(glossy_scene.materials[kSphereMaterialIndex], MaterialPreset::Glossy);

  const Camera camera = make_default_camera(1.0f);
  const RenderSettings settings{16, 16, 0.001f, 1, 1};
  Renderer matte_renderer;
  Renderer glossy_renderer;
  const Image matte = matte_renderer.render(matte_scene, camera, settings);
  const Image glossy = glossy_renderer.render(glossy_scene, camera, settings);

  EXPECT_GT(image_difference(matte, glossy), 1.0e-5f);
}

TEST(CpuRenderer, ClothSheenChangesDirectLighting) {
  Scene matte_scene = make_default_scene();
  apply_material_preset(matte_scene.materials[kSphereMaterialIndex], MaterialPreset::Matte);
  matte_scene.materials[kSphereMaterialIndex].roughness = 0.8f;
  Scene cloth_scene = matte_scene;
  apply_material_preset(cloth_scene.materials[kSphereMaterialIndex], MaterialPreset::Cloth);

  const Camera camera = make_default_camera(1.0f);
  const RenderSettings settings{16, 16, 0.001f, 1, 1};
  Renderer matte_renderer;
  Renderer cloth_renderer;
  const Image matte = matte_renderer.render(matte_scene, camera, settings);
  const Image cloth = cloth_renderer.render(cloth_scene, camera, settings);

  EXPECT_GT(image_difference(matte, cloth), 1.0e-5f);
}

TEST(CpuRenderer, SkinSubsurfaceChangesDirectLighting) {
  Scene matte_scene = make_default_scene();
  apply_material_preset(matte_scene.materials[kSphereMaterialIndex], MaterialPreset::Matte);
  matte_scene.materials[kSphereMaterialIndex].roughness = 0.45f;
  Scene skin_scene = matte_scene;
  apply_material_preset(skin_scene.materials[kSphereMaterialIndex], MaterialPreset::Skin);

  const Camera camera = make_default_camera(1.0f);
  const RenderSettings settings{16, 16, 0.001f, 1, 1};
  Renderer matte_renderer;
  Renderer skin_renderer;
  const Image matte = matte_renderer.render(matte_scene, camera, settings);
  const Image skin = skin_renderer.render(skin_scene, camera, settings);

  EXPECT_GT(image_difference(matte, skin), 1.0e-5f);
}

TEST(CpuRenderer, HairAnisotropyChangesDirectLighting) {
  Scene glossy_scene = make_default_scene();
  apply_material_preset(glossy_scene.materials[kSphereMaterialIndex], MaterialPreset::Glossy);
  glossy_scene.materials[kSphereMaterialIndex].roughness = 0.35f;
  Scene hair_scene = glossy_scene;
  apply_material_preset(hair_scene.materials[kSphereMaterialIndex], MaterialPreset::Hair);

  const Camera camera = make_default_camera(1.0f);
  const RenderSettings settings{16, 16, 0.001f, 1, 1};
  Renderer glossy_renderer;
  Renderer hair_renderer;
  const Image glossy = glossy_renderer.render(glossy_scene, camera, settings);
  const Image hair = hair_renderer.render(hair_scene, camera, settings);

  EXPECT_GT(image_difference(glossy, hair), 1.0e-5f);
}

TEST(CpuRenderer, SupportsSixteenAreaLightSamples) {
  Scene scene = make_default_scene();
  scene.light = make_rect_area_light({4.0f, 35.0f, 45.0f}, scene.sphere.center, {0.0f, -1.0f, 0.0f},
                                     1.0f, 1.0f, {1.0f, 1.0f, 1.0f}, 100.0f);
  RenderSettings settings{8, 8, 0.001f, 1, 16, 1, 2};
  Renderer renderer;

  const Image image = renderer.render(scene, make_default_camera(1.0f), settings);

  EXPECT_TRUE(image_is_finite(image));
}

TEST(RendererBackend, UsesCpuByDefaultAndReportsBackendAvailability) {
  Renderer renderer;

  EXPECT_EQ(renderer.backend_type(), RendererBackendType::Cpu);
  EXPECT_EQ(renderer.backend_label(RendererBackendType::Cpu), "CPU");
  EXPECT_TRUE(renderer.is_backend_available(RendererBackendType::Cpu));
  if (renderer.is_backend_available(RendererBackendType::Cuda)) {
    const std::string label = renderer.backend_label(RendererBackendType::Cuda);
    EXPECT_EQ(label.rfind("GPU (", 0), 0U);
    EXPECT_NE(label, "GPU (unavailable)");
  } else {
    EXPECT_EQ(renderer.backend_label(RendererBackendType::Cuda), "GPU (unavailable)");
  }
  EXPECT_TRUE(renderer.set_backend(RendererBackendType::Cpu));
  EXPECT_EQ(renderer.backend_type(), RendererBackendType::Cpu);
}

TEST(RendererBackend, RejectsUnavailableCudaWithoutChangingBackend) {
  Renderer renderer;
  if (renderer.is_backend_available(RendererBackendType::Cuda)) {
    GTEST_SKIP() << "CUDA is available in this build and environment";
  }

  EXPECT_FALSE(renderer.set_backend(RendererBackendType::Cuda));
  EXPECT_EQ(renderer.backend_type(), RendererBackendType::Cpu);
  EXPECT_EQ(renderer.backend_label(RendererBackendType::Cuda), "GPU (unavailable)");
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