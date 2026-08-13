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
  if (material.type != MaterialType::Diffuse) {
    return {};
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
  return ambient + direct_light;
}

__global__ void render_kernel(Vec3 *pixels,
                              Scene scene,
                              Camera camera,
                              RenderSettings settings) {
  const int x = blockIdx.x * blockDim.x + threadIdx.x;
  const int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= settings.width || y >= settings.height) {
    return;
  }

  const int samples_per_side = static_cast<int>(ceilf(
    sqrtf(static_cast<float>(settings.samples_per_pixel))));
  Vec3 accumulated_color;
  for (int sample_index = 0; sample_index < settings.samples_per_pixel;
      ++sample_index) {
  const int sample_x = sample_index % samples_per_side;
  const int sample_y = sample_index / samples_per_side;
  const float subpixel_x =
    (static_cast<float>(sample_x) + 0.5f) / samples_per_side;
  const float subpixel_y =
    (static_cast<float>(sample_y) + 0.5f) / samples_per_side;
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
    accumulated_color * (1.0f / settings.samples_per_pixel);
}

void check_cuda(cudaError_t error) {
  if (error != cudaSuccess) {
    throw std::runtime_error(cudaGetErrorString(error));
  }
}

} // namespace

Image Renderer::render(const Scene &scene,
                       const Camera &camera,
                       const RenderSettings &settings) const {
  if (settings.width <= 0 || settings.height <= 0 ||
      settings.samples_per_pixel <= 0 || settings.samples_per_pixel > 16 ||
      !is_valid_scene(scene)) {
    throw std::invalid_argument("invalid scene or render settings");
  }

  const std::size_t pixel_count = static_cast<std::size_t>(settings.width) *
                                  static_cast<std::size_t>(settings.height);
  Vec3 *device_pixels = nullptr;
  check_cuda(cudaMalloc(&device_pixels, pixel_count * sizeof(Vec3)));
  const dim3 threads(8, 8);
  const dim3 blocks((settings.width + threads.x - 1) / threads.x,
                   (settings.height + threads.y - 1) / threads.y);
  render_kernel<<<blocks, threads>>>(device_pixels, scene, camera, settings);
  check_cuda(cudaGetLastError());

  Image image;
  image.width = settings.width;
  image.height = settings.height;
  image.pixels.resize(pixel_count);
  check_cuda(cudaMemcpy(image.pixels.data(), device_pixels,
                        pixel_count * sizeof(Vec3), cudaMemcpyDeviceToHost));
  check_cuda(cudaFree(device_pixels));
  return image;
}

} // namespace raypalette
