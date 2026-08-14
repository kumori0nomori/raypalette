#include "render/renderer.hpp"
#include "render/tracer.hpp"

#include <cuda_runtime.h>

#include <stdexcept>

namespace raypalette {
namespace {

__global__ void render_kernel(Vec3* pixels, Scene scene, Camera camera, RenderSettings settings,
                              int sample_offset, int samples_this_frame) {
  const int x = blockIdx.x * blockDim.x + threadIdx.x;
  const int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= settings.width || y >= settings.height)
    return;

  Vec3 accumulated_color;
  const unsigned int pixel_index = static_cast<unsigned int>(y * settings.width + x);
  for (int sample_index = 0; sample_index < samples_this_frame; ++sample_index) {
    const unsigned int global_sample = static_cast<unsigned int>(sample_offset + sample_index);
    const float subpixel_x = detail::sample_unit(pixel_index, global_sample, 0);
    const float subpixel_y = detail::sample_unit(pixel_index, global_sample, 1);
    const float fresnel_random = detail::sample_unit(pixel_index, global_sample, 2);
    const float u = (static_cast<float>(x) + subpixel_x) / settings.width;
    const float v = (static_cast<float>(y) + subpixel_y) / settings.height;
    accumulated_color +=
        detail::trace_color(scene, camera_ray(camera, u, v), settings.minimum_distance, 0,
                            settings.max_bounces, fresnel_random, settings.light_samples_per_frame);
  }
  pixels[y * settings.width + x] = accumulated_color * (1.0f / samples_this_frame);
}

void check_cuda(cudaError_t error) {
  if (error != cudaSuccess)
    throw std::runtime_error(cudaGetErrorString(error));
}

} // namespace

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

  Vec3* device_pixels = nullptr;
  check_cuda(cudaMalloc(&device_pixels, pixel_count * sizeof(Vec3)));
  const dim3 threads(8, 8);
  const dim3 blocks((settings.width + threads.x - 1) / threads.x,
                    (settings.height + threads.y - 1) / threads.y);
  render_kernel<<<blocks, threads>>>(device_pixels, scene, camera, settings, accumulated_samples_,
                                     samples_this_frame);
  check_cuda(cudaGetLastError());

  std::vector<Vec3> frame_pixels(pixel_count);
  check_cuda(cudaMemcpy(frame_pixels.data(), device_pixels, pixel_count * sizeof(Vec3),
                        cudaMemcpyDeviceToHost));
  check_cuda(cudaFree(device_pixels));

  const int old_sample_count = accumulated_samples_;
  const int new_sample_count = old_sample_count + samples_this_frame;
  for (std::size_t index = 0; index < pixel_count; ++index) {
    accumulated_pixels_[index] =
        (accumulated_pixels_[index] * static_cast<float>(old_sample_count) +
         frame_pixels[index] * static_cast<float>(samples_this_frame)) /
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

} // namespace raypalette
