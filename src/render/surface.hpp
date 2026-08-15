#pragma once

#include "render/bsdf.hpp"
#include "render/geometry.hpp"
#include "render/light.hpp"
#include "render/material.hpp"
#include "render/ray.hpp"

namespace raypalette {
namespace detail {

RAYPALETTE_HOST_DEVICE inline Vec3 surface_effective_f0(const PrincipledParameters& material) {
  const float dielectric_f0 = 0.04f * material.specular;
  const Vec3 dielectric_reflectance{dielectric_f0, dielectric_f0, dielectric_f0};
  const Vec3 base_f0 =
      dielectric_reflectance * (1.0f - material.metallic) + material.base_color * material.metallic;
  const Vec3 coat_f0{0.04f, 0.04f, 0.04f};
  return base_f0 * (1.0f - material.coat) + coat_f0 * material.coat;
}

RAYPALETTE_HOST_DEVICE inline float
surface_effective_roughness(const PrincipledParameters& material) {
  return material.roughness * (1.0f - material.coat) + material.coat_roughness * material.coat;
}

RAYPALETTE_HOST_DEVICE inline float
surface_specular_probability(const PrincipledParameters& material) {
  if (material.metallic >= 1.0f) {
    return 1.0f;
  }
  const Vec3 f0 = surface_effective_f0(material);
  const float maximum_f0 = fmaxf(f0.x, fmaxf(f0.y, f0.z));
  return fminf(0.95f, fmaxf(0.05f, maximum_f0));
}

RAYPALETTE_HOST_DEVICE inline float surface_mixture_pdf(const PrincipledParameters& material,
                                                        float diffuse_pdf, float specular_pdf) {
  const float specular_probability = surface_specular_probability(material);
  return (1.0f - specular_probability) * diffuse_pdf + specular_probability * specular_pdf;
}

RAYPALETTE_HOST_DEVICE inline float surface_sheen_factor(const Ray& ray, const HitRecord& record,
                                                         const PrincipledParameters& material,
                                                         const Vec3& direction) {
  const Vec3 view_direction = normalized(-ray.direction);
  const float view_grazing = 1.0f - fmaxf(0.0f, dot(record.normal, view_direction));
  const float light_grazing = 1.0f - fmaxf(0.0f, dot(record.normal, direction));
  const float grazing = fmaxf(view_grazing, light_grazing);
  return material.sheen * powf(grazing, 5.0f);
}

RAYPALETTE_HOST_DEVICE inline float surface_subsurface_cosine(const HitRecord& record,
                                                              const PrincipledParameters& material,
                                                              const Vec3& direction) {
  if (material.subsurface <= 0.0f) {
    return 0.0f;
  }
  const float wrap = 0.5f * material.subsurface;
  const float wrapped_cosine = (dot(record.normal, direction) + wrap) / fmaxf(1.0f, 1.0f + wrap);
  return material.subsurface * fmaxf(0.0f, fminf(1.0f, wrapped_cosine));
}

RAYPALETTE_HOST_DEVICE inline float surface_specular_pdf(const Ray& ray, const HitRecord& record,
                                                         const Vec3& direction,
                                                         const PrincipledParameters& material) {
  const Vec3 view_direction = normalized(-ray.direction);
  const Vec3 half_vector = normalized(view_direction + direction);
  const float v_dot_h = fmaxf(0.0f, dot(view_direction, half_vector));
  return ggx_anisotropic_reflection_pdf(record.normal, record.tangent, half_vector, v_dot_h,
                                        surface_effective_roughness(material), material.anisotropy);
}

RAYPALETTE_HOST_DEVICE inline float surface_power_heuristic(float first_pdf, float second_pdf) {
  const float first_squared = first_pdf * first_pdf;
  const float second_squared = second_pdf * second_pdf;
  return first_squared / fmaxf(1.0e-12f, first_squared + second_squared);
}

RAYPALETTE_HOST_DEVICE inline Vec3
evaluate_surface_light_sample(const Ray& ray, const HitRecord& record,
                             const PrincipledParameters& material,
                             const LightSample& light_sample) {
  const float cosine = fmaxf(0.0f, dot(record.normal, light_sample.direction_to_light));
  const float subsurface_cosine =
      surface_subsurface_cosine(record, material, light_sample.direction_to_light);
  if (cosine <= 0.0f && subsurface_cosine <= 0.0f) {
    return {};
  }

  const float diffuse_pdf = cosine / kPi;
  float specular_pdf = 0.0f;
  Vec3 specular_brdf;
  const Vec3 view_direction = normalized(-ray.direction);
  const Vec3 half_vector = normalized(view_direction + light_sample.direction_to_light);
  const float n_dot_v = fmaxf(0.0f, dot(record.normal, view_direction));
  const float n_dot_h = fmaxf(0.0f, dot(record.normal, half_vector));
  const float v_dot_h = fmaxf(0.0f, dot(view_direction, half_vector));
  if (n_dot_v > 0.0f && n_dot_h > 0.0f && v_dot_h > 1.0e-6f) {
    const float specular_roughness = surface_effective_roughness(material);
    const float distribution = ggx_anisotropic_distribution(
        record.normal, record.tangent, half_vector, specular_roughness, material.anisotropy);
    const float geometry = ggx_geometry(n_dot_v, cosine, specular_roughness);
    const Vec3 fresnel = schlick_fresnel(surface_effective_f0(material), v_dot_h);
    specular_brdf =
        fresnel * (distribution * geometry / fmaxf(1.0e-6f, 4.0f * n_dot_v * cosine));
    specular_pdf =
        ggx_anisotropic_reflection_pdf(record.normal, record.tangent, half_vector, v_dot_h,
                                       specular_roughness, material.anisotropy);
  }

  const float mixture_pdf = surface_mixture_pdf(material, diffuse_pdf, specular_pdf);
  const float mis_weight = light_sample.pdf <= 0.0f
                               ? 1.0f
                               : surface_power_heuristic(light_sample.pdf, mixture_pdf);
  const float sheen_factor =
      surface_sheen_factor(ray, record, material, light_sample.direction_to_light);
  const float diffuse_weight = 1.0f - material.metallic;
  const Vec3 diffuse_brdf =
      material.base_color * (diffuse_weight * cosine + sheen_factor * cosine + subsurface_cosine);
  return (diffuse_brdf + specular_brdf * cosine) * light_sample.radiance * mis_weight;
}

} // namespace detail
} // namespace raypalette
