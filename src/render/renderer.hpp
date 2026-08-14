#pragma once

#include "render/camera.hpp"
#include "render/image.hpp"
#include "render/scene.hpp"

#include <memory>
#include <vector>

namespace raypalette {

struct RenderSettings {
  int width = 32;
  int height = 32;
  float minimum_distance = 0.001f;
  // number of path samples per pixel
  int samples_per_pixel = 1;
  // number of shadow rays per intersection
  int light_samples_per_frame = 4;
  // number of accumulated path samples per pixel
  int target_samples_per_pixel = 1;
  int max_bounces = 1;
};

enum class RendererBackendType { Cpu, Cuda };

class RendererBackend {
public:
  virtual ~RendererBackend() = default;
  virtual Image render_frame(const Scene& scene, const Camera& camera,
                             const RenderSettings& settings, int sample_offset,
                             int samples_this_frame) = 0;
};

std::unique_ptr<RendererBackend> make_cpu_renderer();

#ifdef RAYPALETTE_CUDA_BACKEND
std::unique_ptr<RendererBackend> make_cuda_renderer();
#endif

class Renderer {
public:
  Renderer();
  ~Renderer();
  Renderer(Renderer&&) noexcept;
  Renderer& operator=(Renderer&&) noexcept;

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  Image render(const Scene& scene, const Camera& camera, const RenderSettings& settings);
  void reset_accumulation();
  int accumulated_samples() const;
  bool is_accumulation_complete(const RenderSettings& settings) const;
  void set_backend(RendererBackendType backend_type);
  RendererBackendType backend_type() const;

private:
  std::unique_ptr<RendererBackend> backend_;
  RendererBackendType backend_type_ = RendererBackendType::Cpu;
  std::vector<Vec3> accumulated_pixels_;
  int accumulated_width_ = 0;
  int accumulated_height_ = 0;
  int accumulated_samples_ = 0;
};

} // namespace raypalette
