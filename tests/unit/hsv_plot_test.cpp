#include "ui/hsv_plot.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace raypalette::ui {
namespace {

TEST(HsvPlot, DefaultViewLeavesVerticalAxisCentered) {
  const HsvPlotView view;
  const Vec3 rotated = rotate_hsv_plot_position({0.0f, 0.5f, 0.0f}, view);

  EXPECT_NEAR(rotated.x, 0.0f, 1.0e-6f);
  EXPECT_NEAR(rotated.y, 0.0f, 1.0e-6f);
  EXPECT_NEAR(rotated.z, 0.0f, 1.0e-6f);
}

TEST(HsvPlot, ConvertsHueToCylinderPosition) {
  const Vec3 position = cylinder_position_at_hue(0.0f, 1.0f, 0.75f);

  EXPECT_NEAR(position.x, 1.0f, 1.0e-6f);
  EXPECT_NEAR(position.y, 0.75f, 1.0e-6f);
  EXPECT_NEAR(position.z, 0.0f, 1.0e-6f);
}

TEST(HsvPlot, HueDistanceWrapsAtUnitBoundary) {
  EXPECT_NEAR(hue_distance(0.99f, 0.01f), 0.02f, 1.0e-6f);
  EXPECT_NEAR(hue_distance(0.25f, 0.75f), 0.5f, 1.0e-6f);
}

} // namespace
} // namespace raypalette::ui