#include "render/renderer.hpp"
#include "render/tracer.hpp"

#include <stdexcept>
#include <utility>

namespace raypalette {

Renderer::Renderer() : backend_(make_cpu_renderer()) {}

Renderer::~Renderer() = default;

Renderer::Renderer(Renderer&&) noexcept = default;

Renderer& Renderer::operator=(Renderer&&) noexcept = default;

Image Renderer::render(const Scene& scene, const Camera& camera, const RenderSettings& settings) {
  if (settings.width <= 0 || settings.height <= 0 || settings.samples_per_pixel <= 0 ||
      settings.samples_per_pixel > 16 || settings.light_samples_per_frame <= 0 ||
      settings.light_samples_per_frame > detail::kMaxLightSampleCount ||
      settings.target_samples_per_pixel <= 0 || settings.max_bounces < 0 ||
      !is_valid_scene(scene)) {
    throw std::invalid_argument("invalid scene or render settings");
  }

  const std::size_t pixel_count =
      static_cast<std::size_t>(settings.width) * static_cast<std::size_t>(settings.height);
  if (accumulated_width_ != settings.width || accumulated_height_ != settings.height ||
      accumulated_pixels_.size() != pixel_count) {
    reset_accumulation();
    accumulated_width_ = settings.width;
    accumulated_height_ = settings.height;
    accumulated_pixels_.resize(pixel_count);
  }

  const int remaining_samples = settings.target_samples_per_pixel - accumulated_samples_;
  const int samples_this_frame =
      remaining_samples > 0
          ? (remaining_samples < settings.samples_per_pixel ? remaining_samples
                                                            : settings.samples_per_pixel)
          : 0;
  if (samples_this_frame == 0) {
    return Image{settings.width, settings.height, accumulated_pixels_};
  }

  const Image frame = backend_->render_frame(scene, camera, settings, accumulated_samples_,
                                               samples_this_frame);
  const int old_sample_count = accumulated_samples_;
  const int new_sample_count = old_sample_count + samples_this_frame;
  for (std::size_t index = 0; index < pixel_count; ++index) {
    accumulated_pixels_[index] =
        (accumulated_pixels_[index] * static_cast<float>(old_sample_count) +
         frame.pixels[index] * static_cast<float>(samples_this_frame)) /
        static_cast<float>(new_sample_count);
  }
  accumulated_samples_ = new_sample_count;
  return Image{settings.width, settings.height, accumulated_pixels_};
}

void Renderer::reset_accumulation() {
  accumulated_pixels_.clear();
  accumulated_width_ = 0;
  accumulated_height_ = 0;
  accumulated_samples_ = 0;
}

int Renderer::accumulated_samples() const {
  return accumulated_samples_;
}

bool Renderer::is_accumulation_complete(const RenderSettings& settings) const {
  return accumulated_samples_ >= settings.target_samples_per_pixel;
}

void Renderer::set_backend(RendererBackendType backend_type) {
  if (backend_type == backend_type_) {
    return;
  }
#ifdef RAYPALETTE_CUDA_BACKEND
  if (backend_type == RendererBackendType::Cuda) {
    backend_ = make_cuda_renderer();
  } else {
    backend_ = make_cpu_renderer();
  }
#else
  if (backend_type == RendererBackendType::Cuda) {
    throw std::runtime_error("CUDA backend is not included in this build");
  }
  backend_ = make_cpu_renderer();
#endif
  backend_type_ = backend_type;
  reset_accumulation();
}

RendererBackendType Renderer::backend_type() const {
  return backend_type_;
}

} // namespace raypalette
