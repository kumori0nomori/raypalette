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

float image_mean_luminance(const Image &image) {
  float total = 0.0f;
  for (const Vec3 &pixel : image.pixels) {
    total += 0.2126f * pixel.x + 0.7152f * pixel.y + 0.0722f * pixel.z;
  }
  return total / static_cast<float>(image.pixels.size());
}

bool image_is_finite(const Image &image) {
  for (const Vec3 &pixel : image.pixels) {
    if (!std::isfinite(pixel.x) || !std::isfinite(pixel.y) ||
        !std::isfinite(pixel.z)) {
      return false;
    }
  }
  return true;
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

TEST(CudaRenderer, ReturnsEnvironmentForRayMiss) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  scene.environment.color = {0.25f, 0.5f, 0.75f};
  scene.environment.intensity = 1.0f;
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
  RenderSettings settings{32, 32, 0.001f, 4, 4, 1};
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
  RenderSettings settings{8, 8, 0.001f, 2, 4, 4};
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

TEST(CudaRenderer, ReflectsEnvironmentThroughMetalSphere) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  Material &sphere_material = scene.materials[kSphereMaterialIndex];
  sphere_material.type = MaterialType::Metal;
  sphere_material.base_color = {0.8f, 0.6f, 0.4f};
  scene.environment.intensity = 0.0f;
  scene.light.point.radiant_intensity = 0.0f;
  scene.environment.color = {0.25f, 0.5f, 0.75f};
  scene.environment.intensity = 1.0f;

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
                                      {16, 16, 0.001f, 1, 4, 1, 3});
  for (const Vec3 &pixel : image.pixels) {
    EXPECT_TRUE(std::isfinite(pixel.x));
    EXPECT_TRUE(std::isfinite(pixel.y));
    EXPECT_TRUE(std::isfinite(pixel.z));
  }
}

TEST(CudaRenderer, RendersColoredGlassWithFinitePixels) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  Material &sphere_material = scene.materials[kSphereMaterialIndex];
  sphere_material.type = MaterialType::Dielectric;
  sphere_material.index_of_refraction = 1.5f;
  sphere_material.base_color = {1.0f, 1.0f, 1.0f};
  sphere_material.transmission_color = {0.9f, 0.35f, 0.1f};
  sphere_material.absorption_density = 1.5f;

  Renderer renderer;
  const Image image = renderer.render(scene, make_default_camera(1.0f),
                                      {32, 32, 0.001f, 2, 4, 2, 3});
  bool has_channel_difference = false;
  EXPECT_TRUE(image_is_finite(image));
  for (const Vec3 &pixel : image.pixels) {
    if (fabsf(pixel.x - pixel.z) > 1.0e-4f ||
        fabsf(pixel.y - pixel.z) > 1.0e-4f) {
      has_channel_difference = true;
    }
  }
  EXPECT_TRUE(has_channel_difference);
}

TEST(CudaRenderer, GlassIorChangesGpuImageStatistics) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  Material &glass = scene.materials[kSphereMaterialIndex];
  glass.type = MaterialType::Dielectric;
  glass.base_color = {1.0f, 1.0f, 1.0f};
  scene.environment.color = {0.05f, 0.05f, 0.05f};
  scene.environment.intensity = 1.0f;
  scene.light.point.radiant_intensity = 0.0f;
  const Camera camera = make_default_camera(1.0f);
  const RenderSettings settings{32, 32, 0.001f, 8, 4, 8, 4};

  Renderer low_ior_renderer;
  glass.index_of_refraction = 1.1f;
  const Image low_ior = low_ior_renderer.render(scene, camera, settings);

  Renderer high_ior_renderer;
  glass.index_of_refraction = 2.2f;
  const Image high_ior = high_ior_renderer.render(scene, camera, settings);

  EXPECT_TRUE(image_is_finite(low_ior));
  EXPECT_TRUE(image_is_finite(high_ior));
  EXPECT_GT(fabsf(image_mean_luminance(low_ior) -
                  image_mean_luminance(high_ior)),
            1.0e-5f);
}

TEST(CudaRenderer, GlassAbsorptionChangesGpuImageStatistics) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  Material &glass = scene.materials[kSphereMaterialIndex];
  glass.type = MaterialType::Dielectric;
  glass.index_of_refraction = 1.5f;
  glass.base_color = {1.0f, 1.0f, 1.0f};
  glass.transmission_color = {0.2f, 0.6f, 0.9f};
  scene.environment.color = {0.05f, 0.05f, 0.05f};
  scene.environment.intensity = 1.0f;
  scene.light.point.radiant_intensity = 0.0f;
  const Camera camera = make_default_camera(1.0f);
  const RenderSettings settings{32, 32, 0.001f, 8, 4, 8, 4};

  Renderer clear_renderer;
  glass.absorption_density = 0.0f;
  const Image clear = clear_renderer.render(scene, camera, settings);

  Renderer dense_renderer;
  glass.absorption_density = 3.0f;
  const Image dense = dense_renderer.render(scene, camera, settings);

  EXPECT_TRUE(image_is_finite(clear));
  EXPECT_TRUE(image_is_finite(dense));
  EXPECT_GT(image_mean_luminance(clear), image_mean_luminance(dense));
}

TEST(CudaRenderer, GlassRemainsFiniteAtObliqueCameraAngle) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  Material &glass = scene.materials[kSphereMaterialIndex];
  glass.type = MaterialType::Dielectric;
  glass.index_of_refraction = 1.5f;
  glass.transmission_color = {0.8f, 0.4f, 0.2f};
  glass.absorption_density = 2.0f;
  const Camera camera{{2.8f, 1.8f, 4.0f},
                      {0.0f, 1.0f, 0.0f},
                      {0.0f, 1.0f, 0.0f},
                      {0.0f, 0.0f, 0.0f}};
  Renderer renderer;
  const Image image = renderer.render(scene, camera,
                                      {32, 32, 0.001f, 8, 4, 8, 5});

  EXPECT_TRUE(image_is_finite(image));
}

TEST(CudaRenderer, EmissiveSphereCanIlluminateTheFloor) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  scene.materials[kSphereMaterialIndex].type = MaterialType::Emissive;
  scene.materials[kSphereMaterialIndex].emission_color = {1.0f, 0.1f, 0.02f};
  scene.materials[kSphereMaterialIndex].emission_strength = 8.0f;
  scene.environment.intensity = 0.0f;
  scene.light.point.radiant_intensity = 0.0f;

  Renderer renderer;
  const Image image = renderer.render(scene, make_default_camera(1.0f),
                                      {32, 32, 0.001f, 1, 1, 1});

  bool has_nonblack_floor_pixel = false;
  for (const Vec3 &pixel : image.pixels) {
    if (pixel.x > 0.001f || pixel.y > 0.001f || pixel.z > 0.001f) {
      has_nonblack_floor_pixel = true;
      break;
    }
  }
  EXPECT_TRUE(has_nonblack_floor_pixel);
}

TEST(CudaRenderer, SingleSphereGlassWithEmissionRemainsFinite) {
  if (!has_cuda_device()) {
    GTEST_SKIP() << "No CUDA-capable device is available";
  }
  Scene scene = make_default_scene();
  Material &sphere = scene.materials[kSphereMaterialIndex];
  sphere.type = MaterialType::Dielectric;
  sphere.index_of_refraction = 1.5f;
  sphere.transmission_color = {0.8f, 0.4f, 0.2f};
  sphere.absorption_density = 0.5f;
  sphere.emission_color = {1.0f, 0.1f, 0.02f};
  sphere.emission_strength = 2.0f;
  scene.environment.intensity = 0.0f;
  scene.light.point.radiant_intensity = 0.0f;

  Renderer renderer;
  const Image image = renderer.render(scene, make_default_camera(1.0f),
                                      {32, 32, 0.001f, 2, 4, 2, 4});

  EXPECT_TRUE(image_is_finite(image));
}

} // namespace
} // namespace raypalette