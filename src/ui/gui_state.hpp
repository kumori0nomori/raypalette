#pragma once

#include "ui/hsv_plot.hpp"
#include "ui/palette.hpp"
#include "ui/texture.hpp"

#include <vector>

namespace raypalette::ui {

struct GuiState {
  DisplaySettings display_settings;
  HsvPlotView hsv_plot_view;
  HsvHueSection hsv_hue_section;
  std::vector<PaletteColor> palette;
  int selected_palette_index = -1;
  bool needs_render = true;
  bool needs_display_update = true;
};

} // namespace raypalette::ui
