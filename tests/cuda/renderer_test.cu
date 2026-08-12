#include "render/renderer.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace raypalette {
namespace {

TEST(CudaRenderer, RendersFiniteCanonicalImage) {
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

} // namespace
} // namespace raypalette