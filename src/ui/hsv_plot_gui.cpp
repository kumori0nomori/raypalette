#include "ui/hsv_plot_gui.hpp"

namespace raypalette::ui {

HsvScreenPoint project_hsv_position(const Vec3& position, const ImVec2& center, float scale,
                                    const HsvPlotView& view) {
  const Vec3 rotated = rotate_hsv_plot_position(position, view);
  return {{center.x + rotated.x * scale, center.y - rotated.y * scale}, rotated.z};
}

ImU32 hsv_plot_color(const Vec3& color, float alpha) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, alpha));
}

} // namespace raypalette::ui
