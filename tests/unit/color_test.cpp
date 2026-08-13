#include "math/color.hpp"

#include <gtest/gtest.h>

namespace raypalette {
namespace {

TEST(Color, ConvertsSrgbReferenceValuesToLinear) {
  EXPECT_FLOAT_EQ(srgb_to_linear_component(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(srgb_to_linear_component(1.0f), 1.0f);
  EXPECT_NEAR(srgb_to_linear_component(0.5f), 0.21404114f, 1.0e-6f);
}

TEST(Color, ConvertsLinearReferenceValuesToSrgb) {
  EXPECT_FLOAT_EQ(linear_to_srgb_component(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(linear_to_srgb_component(1.0f), 1.0f);
  EXPECT_NEAR(linear_to_srgb_component(0.21404114f), 0.5f, 1.0e-6f);
}

TEST(Color, ClampsOutOfRangeColorComponents) {
  const Vec3 converted = linear_to_srgb({-1.0f, 0.5f, 2.0f});

  EXPECT_FLOAT_EQ(converted.x, 0.0f);
  EXPECT_NEAR(converted.y, 0.73535698f, 1.0e-6f);
  EXPECT_FLOAT_EQ(converted.z, 1.0f);
}

TEST(Color, AppliesExposureInLinearSpace) {
  const Vec3 color{0.25f, 0.5f, 1.0f};
  const Vec3 brighter = apply_exposure(color, 2.0f);
  const Vec3 darker = apply_exposure(color, -1.0f);

  EXPECT_FLOAT_EQ(brighter.x, 1.0f);
  EXPECT_FLOAT_EQ(brighter.y, 2.0f);
  EXPECT_FLOAT_EQ(darker.z, 0.5f);
}

TEST(Color, CompressesHdrValuesWithReinhardToneMapping) {
  const Vec3 mapped = reinhard_tonemap({0.0f, 1.0f, 9.0f});

  EXPECT_FLOAT_EQ(mapped.x, 0.0f);
  EXPECT_FLOAT_EQ(mapped.y, 0.5f);
  EXPECT_FLOAT_EQ(mapped.z, 0.9f);
}

TEST(Color, PreparesHdrColorForDisplay) {
  const Vec3 display = prepare_for_display({0.0f, 1.0f, 9.0f}, 0.0f, true);

  EXPECT_FLOAT_EQ(display.x, 0.0f);
  EXPECT_NEAR(display.y, 0.73535698f, 1.0e-6f);
  EXPECT_NEAR(display.z, 0.9546872f, 1.0e-6f);
}

} // namespace
} // namespace raypalette
