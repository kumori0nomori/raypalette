#include "ui/palette.hpp"

#include <gtest/gtest.h>

namespace raypalette::ui {
namespace {

TEST(Palette, ConvertsColorToUppercaseHex) {
  EXPECT_EQ(color_to_hex({1.0f, 0.5f, 0.0f}), "#FF8000");
}

TEST(Palette, ClampsColorBeforeConvertingToHex) {
  EXPECT_EQ(color_to_hex({-1.0f, 0.5f, 2.0f}), "#0080FF");
}

TEST(Palette, ComparesColorsAtDisplayPrecision) {
  EXPECT_TRUE(same_palette_color({1.0f, 0.5f, 0.0f}, {1.0f, 0.5f, 0.0f}));
  EXPECT_FALSE(same_palette_color({1.0f, 0.5f, 0.0f}, {0.0f, 0.5f, 1.0f}));
}

} // namespace
} // namespace raypalette::ui