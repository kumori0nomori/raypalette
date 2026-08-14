#include "render/renderer.hpp"
#include "render/tracer.hpp"

#include <cuda_runtime.h>

#include <stdexcept>
#include <utility>

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

class CudaRenderer final : public RendererBackend {
public:
  Image render_frame(const Scene& scene, const Camera& camera, const RenderSettings& settings,
                     int sample_offset, int samples_this_frame) override {
    const std::size_t pixel_count =
        static_cast<std::size_t>(settings.width) * static_cast<std::size_t>(settings.height);
    Vec3* device_pixels = nullptr;
    check_cuda(cudaMalloc(&device_pixels, pixel_count * sizeof(Vec3)));
    const dim3 threads(8, 8);
    const dim3 blocks((settings.width + threads.x - 1) / threads.x,
                      (settings.height + threads.y - 1) / threads.y);
    render_kernel<<<blocks, threads>>>(device_pixels, scene, camera, settings, sample_offset,
                                       samples_this_frame);
    check_cuda(cudaGetLastError());

    std::vector<Vec3> frame_pixels(pixel_count);
    check_cuda(cudaMemcpy(frame_pixels.data(), device_pixels, pixel_count * sizeof(Vec3),
                          cudaMemcpyDeviceToHost));
    check_cuda(cudaFree(device_pixels));
    return Image{settings.width, settings.height, std::move(frame_pixels)};
  }
};

} // namespace

std::unique_ptr<RendererBackend> make_cuda_renderer() {
  return std::make_unique<CudaRenderer>();
}

bool cuda_backend_available() {
  int device_count = 0;
  const cudaError_t error = cudaGetDeviceCount(&device_count);
  if (error != cudaSuccess) {
    cudaGetLastError();
    return false;
  }
  return device_count > 0;
}

} // namespace raypalette
