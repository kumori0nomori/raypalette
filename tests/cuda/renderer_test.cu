#include "render/renderer.hpp"

#include <cuda_runtime.h>
#include <gtest/gtest.h>

#include <cmath>

namespace raypalette {
namespace {

bool has_cuda_device() {
  int device_count = 0;
  const cudaError_t error = cudaGetDeviceCount(&device_count);
  return error == cudaSuccess && device_count > 0;
}

TEST(CudaRenderer, RendersFiniteCanonicalImage) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Renderer renderer;
  const Image image = renderer.render(make_default_scene(),
                                      make_default_camera(1.0f),
                                      {32, 32, 0.001f});

  ASSERT_EQ(image.width, 32);
  ASSERT_EQ(image.height, 32);
  ASSERT_EQ(image.pixels.size(), 1024U);
  for (const Vec3 &pixel : image.pixels) {
    EXPECT_TRUE(std::isfinite(pixel.x));
    EXPECT_TRUE(std::isfinite(pixel.y));
    EXPECT_TRUE(std::isfinite(pixel.z));
  }
}

TEST(CudaRenderer, ReturnsBackgroundForRayMiss) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  scene.background_color = {0.25f, 0.5f, 0.75f};
  const Camera miss_camera{{0.0f, 1.0f, 5.0f},
                           {0.0f, 1.0f, 6.0f},
                           {0.0f, 0.0f, 0.0f},
                           {0.0f, 0.0f, 0.0f}};
  Renderer renderer;
  const Image image = renderer.render(scene, miss_camera, {1, 1, 0.001f});

  EXPECT_NEAR(image.pixels.front().x, 0.25f, 1.0e-6f);
  EXPECT_NEAR(image.pixels.front().y, 0.5f, 1.0e-6f);
  EXPECT_NEAR(image.pixels.front().z, 0.75f, 1.0e-6f);
}

TEST(CudaRenderer, SupportsDeterministicSupersampling) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Renderer renderer;
  RenderSettings settings{32, 32, 0.001f, 4};
  const Image image = renderer.render(make_default_scene(),
                                      make_default_camera(1.0f), settings);

  ASSERT_EQ(image.pixels.size(), 1024U);
  for (const Vec3 &pixel : image.pixels) {
    EXPECT_TRUE(std::isfinite(pixel.x));
    EXPECT_TRUE(std::isfinite(pixel.y));
    EXPECT_TRUE(std::isfinite(pixel.z));
  }
}

TEST(CudaRenderer, AccumulatesProgressiveFrames) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Renderer renderer;
  RenderSettings settings{8, 8, 0.001f, 2, 4};
  const Scene scene = make_default_scene();
  const Camera camera = make_default_camera(1.0f);

  renderer.render(scene, camera, settings);
  EXPECT_EQ(renderer.accumulated_samples(), 2);
  EXPECT_FALSE(renderer.is_accumulation_complete(settings));
  renderer.render(scene, camera, settings);
  EXPECT_EQ(renderer.accumulated_samples(), 4);
  EXPECT_TRUE(renderer.is_accumulation_complete(settings));
}

TEST(CudaRenderer, RendersEmissionWithoutDirectLight) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  Material &sphere_material = scene.materials[kSphereMaterialIndex];
  sphere_material.type = MaterialType::Emissive;
  sphere_material.base_color = {};
  sphere_material.emission_color = {0.2f, 0.4f, 0.8f};
  sphere_material.emission_strength = 2.0f;
  scene.environment.intensity = 0.0f;
  scene.light.point.radiant_intensity = 0.0f;

  Renderer renderer;
  const Image image = renderer.render(scene, make_default_camera(1.0f),
                                      {32, 32, 0.001f, 1, 1});
  const Vec3 &center_pixel = image.pixels[16 * image.width + 16];

  EXPECT_NEAR(center_pixel.x, 0.4f, 1.0e-5f);
  EXPECT_NEAR(center_pixel.y, 0.8f, 1.0e-5f);
  EXPECT_NEAR(center_pixel.z, 1.6f, 1.0e-5f);
}

TEST(CudaRenderer, ReflectsBackgroundThroughMetalSphere) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  Material &sphere_material = scene.materials[kSphereMaterialIndex];
  sphere_material.type = MaterialType::Metal;
  sphere_material.base_color = {0.8f, 0.6f, 0.4f};
  scene.environment.intensity = 0.0f;
  scene.light.point.radiant_intensity = 0.0f;
  scene.background_color = {0.25f, 0.5f, 0.75f};

  Renderer renderer;
  const Image image = renderer.render(scene, make_default_camera(1.0f),
                                      {32, 32, 0.001f, 1, 1, 1});
  const Vec3 &center_pixel = image.pixels[16 * image.width + 16];

  EXPECT_NEAR(center_pixel.x, 0.2f, 1.0e-5f);
  EXPECT_NEAR(center_pixel.y, 0.3f, 1.0e-5f);
  EXPECT_NEAR(center_pixel.z, 0.3f, 1.0e-5f);
}

TEST(CudaRenderer, RendersGlassMaterial) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  Material &sphere_material = scene.materials[kSphereMaterialIndex];
  sphere_material.type = MaterialType::Dielectric;
  sphere_material.index_of_refraction = 1.5f;
  sphere_material.base_color = {1.0f, 1.0f, 1.0f};
  scene.environment.intensity = 0.0f;
  scene.light.point.radiant_intensity = 0.0f;

  Renderer renderer;
  const Image image = renderer.render(scene, make_default_camera(1.0f),
                                      {16, 16, 0.001f, 1, 1, 3});
  for (const Vec3 &pixel : image.pixels) {
    EXPECT_TRUE(std::isfinite(pixel.x));
    EXPECT_TRUE(std::isfinite(pixel.y));
    EXPECT_TRUE(std::isfinite(pixel.z));
  }
}

} // namespace
} // namespace raypalette