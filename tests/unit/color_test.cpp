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

TEST(Color, ConvertsSrgbPrimariesToHsv) {
  const Hsv red = srgb_to_hsv({1.0f, 0.0f, 0.0f});
  const Hsv green = srgb_to_hsv({0.0f, 1.0f, 0.0f});
  const Hsv blue = srgb_to_hsv({0.0f, 0.0f, 1.0f});

  EXPECT_FLOAT_EQ(red.hue, 0.0f);
  EXPECT_FLOAT_EQ(red.saturation, 1.0f);
  EXPECT_FLOAT_EQ(red.value, 1.0f);
  EXPECT_NEAR(green.hue, 1.0f / 3.0f, 1.0e-6f);
  EXPECT_NEAR(blue.hue, 2.0f / 3.0f, 1.0e-6f);
}

TEST(Color, ConvertsAchromaticSrgbToTheCylinderAxis) {
  const Hsv gray = srgb_to_hsv({0.4f, 0.4f, 0.4f});
  const Vec3 position = hsv_cylinder_position(gray);

  EXPECT_FLOAT_EQ(gray.hue, 0.0f);
  EXPECT_FLOAT_EQ(gray.saturation, 0.0f);
  EXPECT_FLOAT_EQ(gray.value, 0.4f);
  EXPECT_FLOAT_EQ(position.x, 0.0f);
  EXPECT_FLOAT_EQ(position.y, 0.4f);
  EXPECT_FLOAT_EQ(position.z, 0.0f);
}

TEST(Color, ConvertsHsvPrimariesToSrgb) {
  const Vec3 red = hsv_to_srgb({0.0f, 1.0f, 1.0f});
  const Vec3 green = hsv_to_srgb({1.0f / 3.0f, 1.0f, 1.0f});
  const Vec3 blue = hsv_to_srgb({2.0f / 3.0f, 1.0f, 1.0f});

  EXPECT_FLOAT_EQ(red.x, 1.0f);
  EXPECT_FLOAT_EQ(red.y, 0.0f);
  EXPECT_FLOAT_EQ(red.z, 0.0f);
  EXPECT_NEAR(green.y, 1.0f, 1.0e-6f);
  EXPECT_NEAR(blue.z, 1.0f, 1.0e-6f);
}

TEST(Color, ConvertsHsvWithoutSaturationToGray) {
  const Vec3 gray = hsv_to_srgb({0.42f, 0.0f, 0.4f});

  EXPECT_FLOAT_EQ(gray.x, 0.4f);
  EXPECT_FLOAT_EQ(gray.y, 0.4f);
  EXPECT_FLOAT_EQ(gray.z, 0.4f);
}

TEST(Color, ClampsSrgbBeforeConvertingToHsv) {
  const Hsv color = srgb_to_hsv({2.0f, -1.0f, 0.5f});

  EXPECT_NEAR(color.hue, 11.0f / 12.0f, 1.0e-6f);
  EXPECT_FLOAT_EQ(color.saturation, 1.0f);
  EXPECT_FLOAT_EQ(color.value, 1.0f);
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
