#pragma once

#include "render/camera.hpp"
#include "render/image.hpp"
#include "render/scene.hpp"

namespace raypalette {

struct RenderSettings {
  int width = 32;
  int height = 32;
  float minimum_distance = 0.001f;
  int samples_per_pixel = 1;
};

class Renderer {
public:
  Image render(const Scene &scene, const Camera &camera,
               const RenderSettings &settings) const;
};

} // namespace raypalette
