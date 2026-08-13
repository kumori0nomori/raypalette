#pragma once

#include "render/camera.hpp"
#include "render/image.hpp"
#include "render/scene.hpp"

#include <vector>

namespace raypalette {

struct RenderSettings {
  int width = 32;
  int height = 32;
  float minimum_distance = 0.001f;
  int samples_per_pixel = 1;
  int target_samples = 1;
};

class Renderer {
public:
  Image render(const Scene &scene, const Camera &camera,
               const RenderSettings &settings);
  void reset_accumulation();
  int accumulated_samples() const;
  bool is_accumulation_complete(const RenderSettings &settings) const;

private:
  std::vector<Vec3> accumulated_pixels_;
  int accumulated_width_ = 0;
  int accumulated_height_ = 0;
  int accumulated_samples_ = 0;
};

} // namespace raypalette
