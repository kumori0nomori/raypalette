#pragma once

#include "render/image.hpp"

#include <GL/gl.h>

#include <vector>

namespace raypalette::ui {

struct DisplaySettings {
  float exposure_ev = 0.0f;
  bool use_reinhard = true;
};

struct Texture {
  GLuint id = 0;
  int width = 0;
  int height = 0;
  std::vector<Vec3> display_pixels;

  void create(int new_width, int new_height);
  void upload(const Image& image, const DisplaySettings& display_settings);
  void destroy();
};

} // namespace raypalette::ui
