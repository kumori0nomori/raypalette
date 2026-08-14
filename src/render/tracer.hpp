#pragma once

#include "render/bsdf.hpp"
#include "render/light_sampling.hpp"
#include "render/renderer.hpp"

namespace raypalette {
namespace detail {

RAYPALETTE_HOST_DEVICE inline unsigned int float_bits(float value) {
  union {
    float value;
    unsigned int bits;
  } representation{value};
  return representation.bits;
}

RAYPALETTE_HOST_DEVICE inline unsigned int sample_hash(unsigned int value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  return value ^ (value >> 16);
}

RAYPALETTE_HOST_DEVICE inline float sample_unit(unsigned int pixel_index, unsigned int sample_index,
                                                unsigned int channel) {
  const unsigned int seed = pixel_index * 9781U + sample_index * 6271U + channel * 26699U;
  return static_cast<float>(sample_hash(seed) & 0x00ffffffU) / 16777216.0f;
}

constexpr int kMaxLightSampleCount = 4;
constexpr float kPi = 3.14159265358979323846f;

RAYPALETTE_HOST_DEVICE inline bool hit_scene(const Scene& scene, const Ray& ray,
                                             float minimum_distance, float maximum_distance,
                                             HitRecord& record) {
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

RAYPALETTE_HOST_DEVICE inline float power_heuristic(float first_pdf, float second_pdf) {
  const float first_squared = first_pdf * first_pdf;
  const float second_squared = second_pdf * second_pdf;
  return first_squared / fmaxf(1.0e-12f, first_squared + second_squared);
}

RAYPALETTE_HOST_DEVICE inline Vec3 emitted_radiance(const Material& material) {
  if (material.type != MaterialType::Emissive) {
    return {};
  }
  return material.emission_color * material.emission_strength;
}

RAYPALETTE_HOST_DEVICE inline Vec3 environment_radiance(const Scene& scene) {
  return scene.environment.color * scene.environment.intensity;
}

RAYPALETTE_HOST_DEVICE inline bool visible_to_light(const Scene& scene, const HitRecord& record,
                                                    const LightSample& light_sample,
                                                    float minimum_distance) {
  const Ray shadow_ray{record.position + minimum_distance * record.normal,
                       light_sample.direction_to_light};
  HitRecord shadow_record;
  return !hit_scene(scene, shadow_ray, minimum_distance, light_sample.distance, shadow_record);
}

RAYPALETTE_HOST_DEVICE inline bool try_sample_light(const Scene& scene, const HitRecord& record,
                                                    int sample_index, LightSample& sample) {
  switch (scene.light.type) {
  case LightType::RectArea: {
    const int sample_x = sample_index % 2;
    const int sample_y = sample_index / 2;
    const float sample_u = (static_cast<float>(sample_x) + 0.5f) * 0.5f - 0.5f;
    const float sample_v = (static_cast<float>(sample_y) + 0.5f) * 0.5f - 0.5f;
    return sample_area_light(scene.light, record.position, sample_u, sample_v, sample);
  }
  case LightType::Point:
  case LightType::Directional:
    return sample_index == 0 && sample_light(scene.light, record.position, sample);
  }
  return false;
}

RAYPALETTE_HOST_DEVICE inline bool try_sample_emissive_sphere(const Scene& scene,
                                                              const HitRecord& record,
                                                              int sample_index, float random_value,
                                                              LightSample& sample) {
  const Material& emissive = scene.materials[kSphereMaterialIndex];
  if (emissive.type != MaterialType::Emissive || record.material_index == kSphereMaterialIndex ||
      emissive.emission_strength <= 0.0f || sample_index < 0 ||
      sample_index >= kMaxLightSampleCount) {
    return false;
  }
  const unsigned int seed =
      float_bits(random_value) + static_cast<unsigned int>(sample_index * 7919);
  const float sample_u = static_cast<float>(sample_hash(seed) & 0x00ffffffU) / 16777216.0f;
  const float sample_v = static_cast<float>(sample_hash(seed + 1U) & 0x00ffffffU) / 16777216.0f;
  const float z = 1.0f - 2.0f * sample_u;
  const float phi = 2.0f * kPi * sample_v;
  const float radial = sqrtf(fmaxf(0.0f, 1.0f - z * z));
  const Vec3 sample_normal{radial * cosf(phi), radial * sinf(phi), z};
  const Vec3 light_position = scene.sphere.center + scene.sphere.radius * sample_normal;
  const Vec3 offset = light_position - record.position;
  const float distance_squared = length_squared(offset);
  if (distance_squared <= 1.0e-12f) {
    return false;
  }
  const float inverse_distance = 1.0f / sqrtf(distance_squared);
  sample.direction_to_light = offset * inverse_distance;
  sample.distance = distance_squared * inverse_distance;
  const float light_cosine = fmaxf(0.0f, dot(sample_normal, -sample.direction_to_light));
  const float area = 4.0f * kPi * scene.sphere.radius * scene.sphere.radius;
  if (light_cosine <= 0.0f || area <= 0.0f) {
    return false;
  }
  sample.radiance = emitted_radiance(emissive) * (light_cosine / distance_squared);
  sample.pdf = distance_squared / (light_cosine * area);
  return true;
}

RAYPALETTE_HOST_DEVICE inline Vec3 cosine_sample_direction(const Vec3& normal, float u, float v) {
  const float radius = sqrtf(v);
  const float phi = 2.0f * kPi * u;
  const Vec3 local{radius * cosf(phi), radius * sinf(phi), sqrtf(fmaxf(0.0f, 1.0f - v))};
  const Vec3 reference = fabsf(normal.x) > 0.9f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
  const Vec3 tangent = normalized(cross(reference, normal));
  return normalized(local.x * tangent + local.y * cross(normal, tangent) + local.z * normal);
}

RAYPALETTE_HOST_DEVICE inline Vec3
evaluate_diffuse_lighting(const Scene& scene, const HitRecord& record, const Material& material,
                          float minimum_distance, int light_sample_count, float random_value) {
  const Vec3 ambient = material.base_color * scene.environment.color * scene.environment.intensity;
  Vec3 direct_light;
  for (int sample_index = 0; sample_index < light_sample_count; ++sample_index) {
    LightSample light_sample;
    if (!try_sample_light(scene, record, sample_index, light_sample))
      continue;
    const float cosine = fmaxf(0.0f, dot(record.normal, light_sample.direction_to_light));
    if (cosine > 0.0f && visible_to_light(scene, record, light_sample, minimum_distance)) {
      const float bsdf_pdf = cosine / kPi;
      const float mis_weight =
          light_sample.pdf <= 0.0f ? 1.0f : power_heuristic(light_sample.pdf, bsdf_pdf);
      direct_light += material.base_color * light_sample.radiance * cosine * mis_weight;
    }
  }
  if (scene.light.type == LightType::RectArea)
    direct_light *= 1.0f / light_sample_count;

  Vec3 emissive_direct_light;
  for (int sample_index = 0; sample_index < light_sample_count; ++sample_index) {
    LightSample light_sample;
    if (!try_sample_emissive_sphere(scene, record, sample_index, random_value, light_sample))
      continue;
    const float cosine = fmaxf(0.0f, dot(record.normal, light_sample.direction_to_light));
    if (cosine > 0.0f && visible_to_light(scene, record, light_sample, minimum_distance)) {
      const float bsdf_pdf = cosine / kPi;
      const float mis_weight = power_heuristic(light_sample.pdf, bsdf_pdf);
      emissive_direct_light += material.base_color * light_sample.radiance * cosine * mis_weight;
    }
  }
  emissive_direct_light *= 1.0f / light_sample_count;
  return ambient + direct_light + emissive_direct_light;
}

RAYPALETTE_HOST_DEVICE inline Vec3
evaluate_metal_lighting(const Scene& scene, const Ray& ray, const HitRecord& record,
                        const Material& material, float minimum_distance, int light_sample_count) {
  const Vec3 view_direction = normalized(-ray.direction);
  const float n_dot_v = fmaxf(0.0f, dot(record.normal, view_direction));
  Vec3 direct_reflection;
  for (int sample_index = 0; sample_index < light_sample_count; ++sample_index) {
    LightSample light_sample;
    if (!try_sample_light(scene, record, sample_index, light_sample))
      continue;
    const float n_dot_l = fmaxf(0.0f, dot(record.normal, light_sample.direction_to_light));
    if (n_dot_l <= 0.0f || n_dot_v <= 0.0f ||
        !visible_to_light(scene, record, light_sample, minimum_distance))
      continue;
    const Vec3 half_vector = normalized(view_direction + light_sample.direction_to_light);
    const float n_dot_h = fmaxf(0.0f, dot(record.normal, half_vector));
    const float v_dot_h = fmaxf(0.0f, dot(view_direction, half_vector));
    const float distribution = ggx_distribution(n_dot_h, material.roughness);
    const float geometry = ggx_geometry(n_dot_v, n_dot_l, material.roughness);
    const Vec3 fresnel = schlick_fresnel(material.base_color, v_dot_h);
    const Vec3 brdf =
        fresnel * (distribution * geometry / fmaxf(1.0e-6f, 4.0f * n_dot_v * n_dot_l));
    const float bsdf_pdf = ggx_reflection_pdf(n_dot_h, v_dot_h, material.roughness);
    const float mis_weight =
        light_sample.pdf <= 0.0f ? 1.0f : power_heuristic(light_sample.pdf, bsdf_pdf);
    direct_reflection += light_sample.radiance * brdf * n_dot_l * mis_weight;
  }
  if (scene.light.type == LightType::RectArea)
    direct_reflection *= 1.0f / light_sample_count;
  return direct_reflection;
}

RAYPALETTE_HOST_DEVICE inline Vec3 next_diffuse_direction(const HitRecord& record,
                                                          float random_value, int bounce,
                                                          float& next_random) {
  const unsigned int random_bits = float_bits(random_value);
  next_random =
      static_cast<float>(sample_hash(random_bits + static_cast<unsigned int>(bounce + 1)) &
                         0x00ffffffU) /
      16777216.0f;
  return cosine_sample_direction(record.normal, random_value, next_random);
}

RAYPALETTE_HOST_DEVICE inline void scatter_dielectric(const Ray& ray, const HitRecord& record,
                                                      const Material& material, float random_value,
                                                      Vec3& direction, Vec3& throughput_factor) {
  const Vec3 unit_direction = normalized(ray.direction);
  const float cos_theta = fminf(dot(-unit_direction, record.normal), 1.0f);
  const float sin_theta = sqrtf(fmaxf(0.0f, 1.0f - cos_theta * cos_theta));
  const float eta_ratio =
      record.front_face ? 1.0f / material.index_of_refraction : material.index_of_refraction;
  const bool total_internal_reflection = eta_ratio * sin_theta > 1.0f;
  const float reflectance = schlick_reflectance(cos_theta, eta_ratio);
  if (total_internal_reflection || random_value < reflectance ||
      !refract_direction(unit_direction, record.normal, eta_ratio, direction)) {
    direction = reflect_direction(unit_direction, record.normal);
  }
  throughput_factor = {1.0f, 1.0f, 1.0f};
  if (!record.front_face) {
    throughput_factor =
        throughput_factor * beer_lambert_attenuation(material.transmission_color,
                                                     material.absorption_density, record.distance);
  }
}

RAYPALETTE_HOST_DEVICE inline void scatter_metal(const Ray& ray, const HitRecord& record,
                                                 const Material& material, float random_value,
                                                 int bounce, Vec3& direction,
                                                 Vec3& throughput_factor, float& next_random,
                                                 float& bsdf_pdf) {
  const Vec3 incoming = normalized(ray.direction);
  if (material.roughness <= 0.001f) {
    direction = reflect_direction(incoming, record.normal);
    throughput_factor = material.base_color;
    next_random = random_value;
    bsdf_pdf = 0.0f;
    return;
  }
  const unsigned int seed = float_bits(random_value) + static_cast<unsigned int>(bounce * 7919);
  const float sample_u = static_cast<float>(sample_hash(seed) & 0x00ffffffU) / 16777216.0f;
  next_random = static_cast<float>(sample_hash(seed + 1U) & 0x00ffffffU) / 16777216.0f;
  const float alpha = material.roughness * material.roughness;
  const float alpha_squared = alpha * alpha;
  const float cos_theta =
      sqrtf((1.0f - next_random) / (1.0f + (alpha_squared - 1.0f) * next_random));
  const float sin_theta = sqrtf(fmaxf(0.0f, 1.0f - cos_theta * cos_theta));
  const float phi = 2.0f * kPi * sample_u;
  const Vec3 local_half{sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta};
  const Vec3 reference =
      fabsf(record.normal.x) > 0.9f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
  const Vec3 tangent = normalized(cross(reference, record.normal));
  const Vec3 half_vector =
      normalized(local_half.x * tangent + local_half.y * cross(record.normal, tangent) +
                 local_half.z * record.normal);
  direction = reflect_direction(incoming, half_vector);
  const Vec3 view_direction = normalized(-incoming);
  const float n_dot_v = fmaxf(0.0f, dot(record.normal, view_direction));
  const float n_dot_l = fmaxf(0.0f, dot(record.normal, direction));
  const float n_dot_h = fmaxf(0.0f, dot(record.normal, half_vector));
  const float v_dot_h = fmaxf(0.0f, dot(view_direction, half_vector));
  if (n_dot_l <= 0.0f || n_dot_v <= 0.0f || n_dot_h <= 0.0f || v_dot_h <= 1.0e-6f) {
    direction = reflect_direction(incoming, record.normal);
    throughput_factor = material.base_color;
    bsdf_pdf = 0.0f;
    return;
  }
  const float geometry = ggx_geometry(n_dot_v, n_dot_l, material.roughness);
  const Vec3 fresnel = schlick_fresnel(material.base_color, v_dot_h);
  throughput_factor = fresnel * (geometry * v_dot_h / fmaxf(1.0e-6f, n_dot_v * n_dot_h));
  bsdf_pdf = ggx_reflection_pdf(n_dot_h, v_dot_h, material.roughness);
}

RAYPALETTE_HOST_DEVICE inline float emissive_sphere_pdf(const Scene& scene, const Ray& ray,
                                                        const HitRecord& record) {
  const Vec3 surface_normal = normalized(record.position - scene.sphere.center);
  const float light_cosine = fmaxf(0.0f, dot(surface_normal, -ray.direction));
  const float area = 4.0f * kPi * scene.sphere.radius * scene.sphere.radius;
  if (light_cosine <= 0.0f || area <= 0.0f)
    return 0.0f;
  return record.distance * record.distance / (light_cosine * area);
}

RAYPALETTE_HOST_DEVICE inline bool apply_russian_roulette(Vec3& throughput, float& random_value,
                                                          int bounce) {
  constexpr int kRouletteStartBounce = 3;
  if (bounce < kRouletteStartBounce)
    return true;
  const float maximum_throughput = fmaxf(throughput.x, fmaxf(throughput.y, throughput.z));
  const float survival_probability = fminf(0.95f, fmaxf(0.05f, maximum_throughput));
  const bool survives = random_value < survival_probability;
  const unsigned int random_bits = float_bits(random_value);
  random_value =
      static_cast<float>(sample_hash(random_bits + static_cast<unsigned int>(bounce * 104729)) &
                         0x00ffffffU) /
      16777216.0f;
  if (!survives)
    return false;
  throughput = throughput * (1.0f / survival_probability);
  return true;
}

RAYPALETTE_HOST_DEVICE inline Vec3 trace_color(const Scene& scene, const Ray& ray,
                                               float minimum_distance, int bounce_count,
                                               int max_bounces, float random_value,
                                               int light_sample_count) {
  Ray current_ray = ray;
  Vec3 throughput{1.0f, 1.0f, 1.0f};
  Vec3 path_radiance;
  float path_random = random_value;
  float previous_bsdf_pdf = 0.0f;
  bool previous_scatter_was_delta = true;
  for (int bounce = bounce_count;; ++bounce) {
    HitRecord record;
    if (!hit_scene(scene, current_ray, minimum_distance, 1.0e30f, record)) {
      path_radiance += throughput * environment_radiance(scene);
      break;
    }
    const Material& material = scene.materials[record.material_index];
    float emission_mis_weight = 1.0f;
    if (material.type == MaterialType::Emissive && !previous_scatter_was_delta &&
        record.material_index == kSphereMaterialIndex) {
      emission_mis_weight =
          power_heuristic(previous_bsdf_pdf, emissive_sphere_pdf(scene, current_ray, record));
    }
    path_radiance += throughput * emitted_radiance(material) * emission_mis_weight;
    if (material.type == MaterialType::Emissive)
      break;
    switch (material.type) {
    case MaterialType::Diffuse: {
      path_radiance +=
          throughput * evaluate_diffuse_lighting(scene, record, material, minimum_distance,
                                                 light_sample_count, path_random);
      if (bounce >= max_bounces || !apply_russian_roulette(throughput, path_random, bounce)) {
        return path_radiance;
      }
      const Vec3 scattered_direction =
          next_diffuse_direction(record, path_random, bounce, path_random);
      throughput = throughput * material.base_color;
      previous_bsdf_pdf = fmaxf(0.0f, dot(record.normal, scattered_direction)) / kPi;
      previous_scatter_was_delta = false;
      current_ray = {record.position + minimum_distance * record.normal, scattered_direction};
      continue;
    }
    case MaterialType::Metal: {
      path_radiance += throughput * evaluate_metal_lighting(scene, current_ray, record, material,
                                                            minimum_distance, light_sample_count);
      if (bounce >= max_bounces || !apply_russian_roulette(throughput, path_random, bounce)) {
        return path_radiance;
      }
      Vec3 direction;
      Vec3 throughput_factor;
      float next_random;
      float bsdf_pdf;
      scatter_metal(current_ray, record, material, path_random, bounce, direction,
                    throughput_factor, next_random, bsdf_pdf);
      throughput = throughput * throughput_factor;
      previous_bsdf_pdf = bsdf_pdf;
      previous_scatter_was_delta = bsdf_pdf <= 0.0f;
      current_ray = {record.position + minimum_distance * record.normal, direction};
      path_random = next_random;
      continue;
    }
    case MaterialType::Dielectric: {
      if (bounce >= max_bounces || !apply_russian_roulette(throughput, path_random, bounce)) {
        return path_radiance;
      }
      Vec3 direction;
      Vec3 throughput_factor;
      scatter_dielectric(current_ray, record, material, path_random, direction, throughput_factor);
      throughput = throughput * throughput_factor;
      const float offset_sign = dot(direction, record.normal) > 0.0f ? 1.0f : -1.0f;
      current_ray = {record.position + offset_sign * minimum_distance * record.normal, direction};
      const unsigned int random_bits = float_bits(path_random);
      path_random =
          static_cast<float>(sample_hash(random_bits + static_cast<unsigned int>(bounce + 1)) &
                             0x00ffffffU) /
          16777216.0f;
      break;
    }
    default:
      return path_radiance;
    }
  }
  return path_radiance;
}

} // namespace detail
} // namespace raypalette
