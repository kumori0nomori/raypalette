#include "render/renderer.hpp"
#include "render/tracer.hpp"

#include <stdexcept>

namespace raypalette {
namespace {

class CpuRenderer final : public RendererBackend {
public:
  Image render_frame(const Scene& scene, const Camera& camera, const RenderSettings& settings,
                     int sample_offset, int samples_this_frame) override {
    const std::size_t pixel_count =
        static_cast<std::size_t>(settings.width) * static_cast<std::size_t>(settings.height);
    std::vector<Vec3> frame_pixels(pixel_count);
    for (int y = 0; y < settings.height; ++y) {
      for (int x = 0; x < settings.width; ++x) {
        Vec3 accumulated_color;
        const unsigned int pixel_index = static_cast<unsigned int>(y * settings.width + x);
        for (int sample_index = 0; sample_index < samples_this_frame; ++sample_index) {
          const unsigned int global_sample =
              static_cast<unsigned int>(sample_offset + sample_index);
          const float subpixel_x = detail::sample_unit(pixel_index, global_sample, 0);
          const float subpixel_y = detail::sample_unit(pixel_index, global_sample, 1);
          const float fresnel_random = detail::sample_unit(pixel_index, global_sample, 2);
          const float u = (static_cast<float>(x) + subpixel_x) / settings.width;
          const float v = (static_cast<float>(y) + subpixel_y) / settings.height;
          accumulated_color += detail::trace_color(
              scene, camera_ray(camera, u, v), settings.minimum_distance, 0, settings.max_bounces,
              fresnel_random, settings.light_samples_per_frame);
        }
        frame_pixels[static_cast<std::size_t>(y) * settings.width + x] =
            accumulated_color * (1.0f / samples_this_frame);
      }
    }
    return Image{settings.width, settings.height, std::move(frame_pixels)};
  }
};

} // namespace

std::unique_ptr<RendererBackend> make_cpu_renderer() {
  return std::make_unique<CpuRenderer>();
}

} // namespace raypalette
