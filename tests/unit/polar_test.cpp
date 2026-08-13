#include "math/polar.hpp"

#include <gtest/gtest.h>

namespace raypalette {
namespace {

TEST(PolarCoordinates, PlacesThetaZeroOnPositiveY) {
  const Vec3 position = polar_to_cartesian(3.0f, 0.0f, 75.0f);

  EXPECT_NEAR(position.x, 0.0f, 1.0e-6f);
  EXPECT_NEAR(position.y, 3.0f, 1.0e-6f);
  EXPECT_NEAR(position.z, 0.0f, 1.0e-6f);
}

TEST(PolarCoordinates, UsesPhiAroundTheYUpAxis) {
  const Vec3 x_axis = polar_to_cartesian(2.0f, 90.0f, 0.0f);
  const Vec3 z_axis = polar_to_cartesian(2.0f, 90.0f, 90.0f);

  EXPECT_NEAR(x_axis.x, 2.0f, 1.0e-6f);
  EXPECT_NEAR(x_axis.y, 0.0f, 1.0e-6f);
  EXPECT_NEAR(x_axis.z, 0.0f, 1.0e-6f);
  EXPECT_NEAR(z_axis.x, 0.0f, 1.0e-6f);
  EXPECT_NEAR(z_axis.y, 0.0f, 1.0e-6f);
  EXPECT_NEAR(z_axis.z, 2.0f, 1.0e-6f);
}

TEST(PolarCoordinates, SupportsZeroRadius) {
  const Vec3 position = polar_to_cartesian(0.0f, 45.0f, 45.0f);

  EXPECT_FLOAT_EQ(position.x, 0.0f);
  EXPECT_FLOAT_EQ(position.y, 0.0f);
  EXPECT_FLOAT_EQ(position.z, 0.0f);
}

} // namespace
} // namespace raypalette