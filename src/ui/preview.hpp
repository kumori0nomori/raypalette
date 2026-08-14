#pragma once

#include "render/image.hpp"
#include "ui/palette.hpp"
#include "ui/texture.hpp"

#include <vector>

namespace raypalette::ui {

struct PreviewLayout {
  float width;
  float height;
};

struct PreviewContext {
  const Image& image;
  const Texture& texture;
  std::vector<PaletteColor>& palette;
  int& selected_palette_index;
  PreviewLayout layout;
};

void draw_preview(PreviewContext& context);

} // namespace raypalette::ui