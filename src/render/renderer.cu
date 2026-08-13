#include "render/renderer.hpp"

#include <cuda_runtime.h>

#include <stdexcept>

namespace raypalette {
namespace {

__device__ bool hit_scene(const Scene &scene,
                          const Ray &ray,
                          float minimum_distance,
                          float maximum_distance,
                          HitRecord &record) {
  bool hit_anything = false;
  float closest_distance = maximum_distance;
  HitRecord candidate;
  if (hit_sphere(scene.sphere, ray, minimum_distance, closest_distance, candidate)) {
    record = candidate;
    closest_distance = candidate.distance;
    hit_anything = true;
  }
  if (hit_plane(scene.floor, ray, minimum_distance, closest_distance, candidate)) {
    record = candidate;
    hit_anything = true;
  }
  return hit_anything;
}

__device__ Vec3 shade(const Scene &scene,
                      const Ray &ray,
                      const HitRecord &record,
                      float minimum_distance) {
  const Material &material = scene.materials[record.material_index];
  const Vec3 emitted = material.emission_color * material.emission_strength;
  if (material.type == MaterialType::Emissive) {
    return emitted;
  }
  if (material.type != MaterialType::Diffuse) {
    return emitted;
  }

  const Vec3 ambient = material.base_color * scene.environment.color *
                       scene.environment.intensity;
  constexpr int area_sample_count = 4;
  Vec3 direct_light;
  for (int sample_index = 0; sample_index < area_sample_count; ++sample_index) {
    LightSample light_sample;
    bool has_sample = false;
    if (scene.light.type == LightType::RectArea) {
      const int sample_x = sample_index % 2;
      const int sample_y = sample_index / 2;
      const float sample_u = (static_cast<float>(sample_x) + 0.5f) * 0.5f - 0.5f;
      const float sample_v = (static_cast<float>(sample_y) + 0.5f) * 0.5f - 0.5f;
      has_sample = sample_area_light(scene.light, record.position, sample_u,
                                     sample_v, light_sample);
    } else if (sample_index == 0) {
      has_sample = sample_light(scene.light, record.position, light_sample);
    }
    if (!has_sample) {
      continue;
    }

    const float cosine =
        fmaxf(0.0f, dot(record.normal, light_sample.direction_to_light));
    if (cosine <= 0.0f) {
      continue;
    }
    const Ray shadow_ray{record.position + minimum_distance * record.normal,
                         light_sample.direction_to_light};
    HitRecord shadow_record;
    if (!hit_scene(scene, shadow_ray, minimum_distance, light_sample.distance,
                   shadow_record)) {
      direct_light += material.base_color * light_sample.radiance * cosine;
    }
  }
  if (scene.light.type == LightType::RectArea) {
    direct_light *= 1.0f / area_sample_count;
  }
  return emitted + ambient + direct_light;
}

// Function to generate a pseudo-random sample value
// based on pixel index, sample index, and channel.
__device__ unsigned int sample_hash(unsigned int value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  return value ^ (value >> 16);
}

// Function to generate a pseudo-random sample value in the range [0, 1).
__device__ float sample_unit(unsigned int pixel_index, unsigned int sample_index,
                             unsigned int channel) {
  const unsigned int seed = pixel_index * 9781U + sample_index * 6271U +
                            channel * 26699U;
  return static_cast<float>(sample_hash(seed) & 0x00ffffffU) / 16777216.0f;
}

__global__ void render_kernel(Vec3 *pixels,
                              Scene scene,
                              Camera camera,
                              RenderSettings settings,
                              int sample_offset,
                              int samples_this_frame) {
  const int x = blockIdx.x * blockDim.x + threadIdx.x;
  const int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= settings.width || y >= settings.height) {
    return;
  }

  Vec3 accumulated_color;
  const unsigned int pixel_index = static_cast<unsigned int>(y * settings.width + x);
  for (int sample_index = 0; sample_index < samples_this_frame; ++sample_index) {
    const unsigned int global_sample =
        static_cast<unsigned int>(sample_offset + sample_index);
    const float subpixel_x = sample_unit(pixel_index, global_sample, 0);
    const float subpixel_y = sample_unit(pixel_index, global_sample, 1);
    const float u = (static_cast<float>(x) + subpixel_x) / settings.width;
    const float v = (static_cast<float>(y) + subpixel_y) / settings.height;
    const Ray ray = camera_ray(camera, u, v);
    HitRecord record;
    accumulated_color +=
      hit_scene(scene, ray, settings.minimum_distance, 1.0e30f, record)
        ? shade(scene, ray, record, settings.minimum_distance)
        : scene.background_color;
  }
  pixels[y * settings.width + x] =
    accumulated_color * (1.0f / samples_this_frame);
}

void check_cuda(cudaError_t error) {
  if (error != cudaSuccess) {
    throw std::runtime_error(cudaGetErrorString(error));
  }
}

} // namespace

Image Renderer::render(const Scene &scene,
                       const Camera &camera,
             const RenderSettings &settings) {
  if (settings.width <= 0 || settings.height <= 0 ||
      settings.samples_per_pixel <= 0 || settings.samples_per_pixel > 16 ||
    settings.target_samples <= 0 ||
      !is_valid_scene(scene)) {
    throw std::invalid_argument("invalid scene or render settings");
  }

  const std::size_t pixel_count = static_cast<std::size_t>(settings.width) *
                                  static_cast<std::size_t>(settings.height);
  if (accumulated_width_ != settings.width ||
      accumulated_height_ != settings.height ||
      accumulated_pixels_.size() != pixel_count) {
    reset_accumulation();
    accumulated_width_ = settings.width;
    accumulated_height_ = settings.height;
    accumulated_pixels_.resize(pixel_count);
  }

  const int remaining_samples = settings.target_samples - accumulated_samples_;
  const int samples_this_frame =
      remaining_samples > 0
          ? (remaining_samples < settings.samples_per_pixel
                 ? remaining_samples
                 : settings.samples_per_pixel)
          : 0;
  if (samples_this_frame == 0) {
    Image image{settings.width, settings.height, accumulated_pixels_};
    return image;
  }

  Vec3 *device_pixels = nullptr;
  check_cuda(cudaMalloc(&device_pixels, pixel_count * sizeof(Vec3)));
  const dim3 threads(8, 8);
  const dim3 blocks((settings.width + threads.x - 1) / threads.x,
                   (settings.height + threads.y - 1) / threads.y);
  render_kernel<<<blocks, threads>>>(device_pixels, scene, camera, settings,
                                     accumulated_samples_, samples_this_frame);
  check_cuda(cudaGetLastError());

  std::vector<Vec3> frame_pixels(pixel_count);
  check_cuda(cudaMemcpy(frame_pixels.data(), device_pixels,
                        pixel_count * sizeof(Vec3), cudaMemcpyDeviceToHost));
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

int Renderer::accumulated_samples() const { return accumulated_samples_; }

bool Renderer::is_accumulation_complete(
    const RenderSettings &settings) const {
  return accumulated_samples_ >= settings.target_samples;
}

} // namespace raypalette
