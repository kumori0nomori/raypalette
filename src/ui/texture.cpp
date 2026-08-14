#include "ui/texture.hpp"

#include "math/color.hpp"

namespace raypalette::ui {

void Texture::create(int new_width, int new_height) {
  width = new_width;
  height = new_height;
  if (id == 0) {
    glGenTextures(1, &id);
  }
  glBindTexture(GL_TEXTURE_2D, id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
}

void Texture::upload(const Image& image, const DisplaySettings& display_settings) {
  if (image.width != width || image.height != height) {
    create(image.width, image.height);
  }
  display_pixels.resize(image.pixels.size());
  for (std::size_t index = 0; index < image.pixels.size(); ++index) {
    display_pixels[index] = prepare_for_display(image.pixels[index], display_settings.exposure_ev,
                                                display_settings.use_reinhard);
  }
  glBindTexture(GL_TEXTURE_2D, id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image.width, image.height, GL_RGB, GL_FLOAT,
                  display_pixels.data());
}

void Texture::destroy() {
  if (id != 0) {
    glDeleteTextures(1, &id);
    id = 0;
  }
}

} // namespace raypalette::ui
