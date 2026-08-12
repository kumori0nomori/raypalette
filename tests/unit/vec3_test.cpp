#include "math/vec3.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace raypalette {
namespace {

TEST(Vec3, AddsSubtractsAndScales) {
  const Vec3 first{1.0f, -2.0f, 3.0f};
  const Vec3 second{4.0f, 5.0f, -6.0f};

  EXPECT_FLOAT_EQ((first + second).x, 5.0f);
  EXPECT_FLOAT_EQ((first + second).y, 3.0f);
  EXPECT_FLOAT_EQ((first - second).z, 9.0f);
  EXPECT_FLOAT_EQ((2.0f * first).y, -4.0f);
}

TEST(Vec3, CalculatesDotAndCrossProducts) {
  const Vec3 x_axis{1.0f, 0.0f, 0.0f};
  const Vec3 y_axis{0.0f, 1.0f, 0.0f};
  const Vec3 perpendicular = cross(x_axis, y_axis);

  EXPECT_FLOAT_EQ(dot(x_axis, y_axis), 0.0f);
  EXPECT_FLOAT_EQ(perpendicular.x, 0.0f);
  EXPECT_FLOAT_EQ(perpendicular.y, 0.0f);
  EXPECT_FLOAT_EQ(perpendicular.z, 1.0f);
}

TEST(Vec3, NormalizesAndGuardsZeroVector) {
  const Vec3 unit = normalized({3.0f, 0.0f, 4.0f});
  const Vec3 zero = normalized({});

  EXPECT_NEAR(length(unit), 1.0f, 1.0e-6f);
  EXPECT_FLOAT_EQ(zero.x, 0.0f);
  EXPECT_FLOAT_EQ(zero.y, 0.0f);
  EXPECT_FLOAT_EQ(zero.z, 0.0f);
}

TEST(Vec3, DetectsFiniteValues) {
  EXPECT_TRUE(is_finite({1.0f, 0.0f, -1.0f}));
  EXPECT_FALSE(is_finite({std::numeric_limits<float>::infinity(), 0.0f, 0.0f}));
}

} // namespace
} // namespace raypalette
