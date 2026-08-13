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

__device__ Vec3 trace_color(const Scene &scene,
                            const Ray &ray,
                            float minimum_distance,
                            int bounce_count,
                            int max_bounces);

constexpr int kAreaLightSampleCount = 4;

__device__ Vec3 emitted_radiance(const Material &material) {
  return material.emission_color * material.emission_strength;
}

__device__ bool visible_to_light(const Scene &scene,
                                 const HitRecord &record,
                                 const LightSample &light_sample,
                                 float minimum_distance) {
  const Ray shadow_ray{record.position + minimum_distance * record.normal,
                       light_sample.direction_to_light};
  HitRecord shadow_record;
  return !hit_scene(scene,
                    shadow_ray,
                    minimum_distance,
                    light_sample.distance,
                    shadow_record);
}

// Function to try sampling a light source in the scene based on the sample index.
// This function is mainly for RectArea lights, which require multiple samples.
// For Point and Directional lights, only one sample is needed,
// and the function will return true only for sample_index == 0.
__device__ bool try_sample_light(const Scene &scene,
                                 const HitRecord &record,
                                 int sample_index,
                                 LightSample &sample) {
  switch (scene.light.type) {
    case LightType::RectArea: {
      const int sample_x = sample_index % 2;
      const int sample_y = sample_index / 2;
      const float sample_u = (static_cast<float>(sample_x) + 0.5f) * 0.5f - 0.5f;
      const float sample_v = (static_cast<float>(sample_y) + 0.5f) * 0.5f - 0.5f;
      return sample_area_light(scene.light, 
                               record.position,
                               sample_u,
                               sample_v,
                               sample);
    }
    case LightType::Point:
    case LightType::Directional:
      // Point and directional lights require only one sample.
      return sample_index == 0 && sample_light(scene.light, 
                                               record.position,
                                               sample);
  }
  return false;
}

__device__ Vec3 shade_diffuse(const Scene &scene, const HitRecord &record,
                              float minimum_distance) {
  const Material &material = scene.materials[record.material_index];
  const Vec3 ambient = material.base_color * scene.environment.color *
                       scene.environment.intensity;
  Vec3 direct_light;
  for (int sample_index = 0; sample_index < kAreaLightSampleCount; ++sample_index) {
    LightSample light_sample;
    if (!try_sample_light(scene, record, sample_index, light_sample)) {
      continue;
    }
    const float cosine =
      fmaxf(0.0f, dot(record.normal, light_sample.direction_to_light));
    if (cosine > 0.0f && visible_to_light(scene,
                                          record, 
                                          light_sample,
                                          minimum_distance)) {
      direct_light += material.base_color * light_sample.radiance * cosine;
    }
  }
  if (scene.light.type == LightType::RectArea) {
    direct_light *= 1.0f / kAreaLightSampleCount;
  }
  return emitted_radiance(material) + ambient + direct_light;
}

__device__ Vec3 shade_metal(const Scene &scene,
                            const Ray &ray,
                            const HitRecord &record,
                            float minimum_distance,
                            int bounce_count,
                            int max_bounces) {
  const Material &material = scene.materials[record.material_index];
  // Compute the view direction and the dot product with the surface normal.
  const Vec3 view_direction = normalized(-ray.direction);
  const float n_dot_v = fmaxf(0.0f, dot(record.normal, view_direction));

  // Compute the direct reflection from the light source(s).
  Vec3 direct_reflection;
  for (int sample_index = 0; sample_index < kAreaLightSampleCount; ++sample_index) {
    LightSample light_sample;
    if (!try_sample_light(scene, record, sample_index, light_sample)) {
      continue;
    }
    // Compute the dot products for the shading calculations.
    const float n_dot_l =
        fmaxf(0.0f, dot(record.normal, light_sample.direction_to_light));
    if (n_dot_l <= 0.0f ||
        n_dot_v <= 0.0f ||
        !visible_to_light(scene, record, light_sample, minimum_distance)) {
      continue;
    }

    // Compute the half-vector and the dot products for the GGX shading model.
    // H = (V + L) / |V + L|
    const Vec3 half_vector =
        normalized(view_direction + light_sample.direction_to_light);
    // GGX shading model calculations.
    const float n_dot_h = fmaxf(0.0f, dot(record.normal, half_vector));
    const float v_dot_h = fmaxf(0.0f, dot(view_direction, half_vector));
    const float distribution = ggx_distribution(n_dot_h, material.roughness);
    const float geometry = ggx_geometry(n_dot_v, n_dot_l, material.roughness);
    const Vec3 fresnel = schlick_fresnel(material.base_color, v_dot_h);
    // Compute the BRDF value using the GGX shading model.
    // fr = (F * D * G) / (4 * NdotV * NdotL)
    const Vec3 brdf = fresnel * (distribution * geometry /
                                 fmaxf(1.0e-6f, 4.0f * n_dot_v * n_dot_l));
    direct_reflection += light_sample.radiance * brdf * n_dot_l;
  }
  if (scene.light.type == LightType::RectArea) {
    direct_reflection *= 1.0f / kAreaLightSampleCount;
  }

  // Combine the emitted and reflected light.
  Vec3 reflected = emitted_radiance(material) + direct_reflection;
  if (bounce_count < max_bounces) {
    const Vec3 reflected_direction =
      reflect_direction(normalized(ray.direction), record.normal);
    const Ray reflected_ray{record.position + minimum_distance * record.normal,
                            reflected_direction};
    reflected += material.base_color *
                 trace_color(scene, reflected_ray, minimum_distance,
                             bounce_count + 1, max_bounces);
  }
  return reflected;
}

__device__ Vec3 shade(const Scene &scene, const Ray &ray,
                      const HitRecord &record, float minimum_distance,
                      int bounce_count, int max_bounces) {
  const Material &material = scene.materials[record.material_index];
  switch (material.type) {
    case MaterialType::Emissive:
      return emitted_radiance(material);

    case MaterialType::Metal:
      return shade_metal(scene, ray, record, minimum_distance, bounce_count,
                         max_bounces);

    case MaterialType::Diffuse:
      return shade_diffuse(scene, record, minimum_distance);

    case MaterialType::Dielectric:
      // TODO: implement dielectric shading.
      return emitted_radiance(material);
  }
  return emitted_radiance(material);
}

__device__ Vec3 trace_color(const Scene &scene,
                            const Ray &ray,
                            float minimum_distance,
                            int bounce_count,
                            int max_bounces) {
  HitRecord record;
  if (!hit_scene(scene, ray, minimum_distance, 1.0e30f, record)) {
    return scene.background_color;
  }
  return shade(scene, ray, record, minimum_distance, bounce_count,
               max_bounces);
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
__device__ float sample_unit(unsigned int pixel_index,
                             unsigned int sample_index,
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
    accumulated_color += trace_color(scene, ray, settings.minimum_distance, 0,
                                     settings.max_bounces);
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
  if (settings.width <= 0 ||
      settings.height <= 0 ||
      settings.samples_per_pixel <= 0 ||
      settings.samples_per_pixel > 16 ||
      settings.target_samples <= 0 ||
      settings.max_bounces < 0 ||
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
