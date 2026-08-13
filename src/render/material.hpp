#pragma once

#include "math/color.hpp"

#include <cstdint>

namespace raypalette {

enum class MaterialType : std::uint32_t {
  Diffuse,
  Metal,
  Dielectric,
  Emissive,
};

struct Material {
  MaterialType type = MaterialType::Diffuse;
  Vec3 base_color{0.8f, 0.8f, 0.8f};
  float roughness = 0.5f;
  float index_of_refraction = 1.5f;
  Vec3 emission_color{};
  float emission_strength = 0.0f;
  Vec3 transmission_color{1.0f, 1.0f, 1.0f};
  float absorption_density = 0.0f;
};

//
// Compute the reflected direction and the refracted direction for a dielectric material.
// - Reflection: light reflects off an object's surface like a mirror. (metal)
// - Refraction: light bends as it passes through a transparent material. (glass)
//

RAYPALETTE_HOST_DEVICE constexpr Vec3 reflect_direction(const Vec3 &incoming,
                                                        const Vec3 &normal) {
  return incoming - 2.0f * dot(incoming, normal) * normal;
}

RAYPALETTE_HOST_DEVICE inline bool refract_direction(const Vec3 &incoming,
                                                     const Vec3 &normal,
                                                     float eta_ratio,
                                                     Vec3 &refracted) {
  // Normalize the incoming direction and compute the cosine of the angle between
  // the incoming ray and the surface normal.
  const Vec3 unit_incoming = normalized(incoming);
  const float cos_theta = fminf(dot(-unit_incoming, normal), 1.0f);

  // Compute the perpendicular and parallel components of the refracted ray.
  const Vec3 perpendicular = eta_ratio * (unit_incoming + cos_theta * normal);
  const float parallel_squared = 1.0f - length_squared(perpendicular);
  // If the parallel component is negative, total internal reflection occurs.
  if (parallel_squared < 0.0f) {
    return false;
  }
  refracted = perpendicular - sqrtf(parallel_squared) * normal;
  return true;
}

// Beer-Lambert law for light absorption in a medium.
// T = exp(-sigma * d)
// where T is the transmittance, sigma is the absorption coefficient,
// and d is the distance traveled through the medium.
RAYPALETTE_HOST_DEVICE inline Vec3 beer_lambert_attenuation(const Vec3 &transmission_color,
                                                            float absorption_density,
                                                            float distance) {
  const float minimum_transmission = 1.0e-6f;
  const float red = -logf(fmaxf(minimum_transmission, transmission_color.x));
  const float green = -logf(fmaxf(minimum_transmission, transmission_color.y));
  const float blue = -logf(fmaxf(minimum_transmission, transmission_color.z));
  const float scale = fmaxf(0.0f, absorption_density) * fmaxf(0.0f, distance);
  return {expf(-red * scale), expf(-green * scale), expf(-blue * scale)};
}

// Dielectric (glass) reflectance probability from IOR using Schlick's approximation.
// Returns a scalar used to choose reflection versus refraction.
// R(theta) = R0 + (1 - R0) * (1 - cos(theta))^5
// where R0 = ((1- eta) / (1 + eta))^2
RAYPALETTE_HOST_DEVICE inline float schlick_reflectance(float cosine,
                                                        float index_of_refraction) {
  float reflectance = (1.0f - index_of_refraction) /
                      (1.0f + index_of_refraction);
  reflectance *= reflectance;
  return reflectance + (1.0f - reflectance) * powf(1.0f - cosine, 5.0f);
}

// Metallic Fresnel reflectance using an RGB base reflectance (F0).
// Returns a colored reflection factor for the GGX BRDF.
// F = F0 + (1 - F0) * (1 - cos(theta))^5
// Reference:
// - https://ja.wikipedia.org/wiki/%E3%83%95%E3%83%AC%E3%83%8D%E3%83%AB%E3%81%AE%E5%BC%8F
RAYPALETTE_HOST_DEVICE inline Vec3 schlick_fresnel(const Vec3 &f0,
                                                   float cosine) {
  const float factor = powf(1.0f - fmaxf(0.0f, fminf(1.0f, cosine)), 5.0f);
  return f0 + (Vec3{1.0f, 1.0f, 1.0f} - f0) * factor;
}

// GGX microfacet distribution function. (Trowbridge-Reitz (GGX) NDF)
// D(h) = alpha^2 / (pi * ((n.h)^2 * (alpha^2 - 1) + 1)^2)
// Reference:
// - https://hanecci.hatenadiary.org/entry/20130511/p1
RAYPALETTE_HOST_DEVICE inline float ggx_distribution(float n_dot_h,
                                                     float roughness) {
  constexpr float pi = 3.14159265358979323846f;
  const float alpha = fmaxf(0.001f, roughness * roughness);
  const float alpha_squared = alpha * alpha;
  const float n_dot_h_squared = n_dot_h * n_dot_h;
  const float denominator =
      n_dot_h_squared * (alpha_squared - 1.0f) + 1.0f;
  return alpha_squared / (pi * denominator * denominator);
}

// Approximating shadowing and masking by microfacets. (Schlick-GGX model)
// G(v) = n.v / (n.v * (1 - k) + k)
// where k = (alpha + 1)^2 / 8
RAYPALETTE_HOST_DEVICE inline float ggx_geometry_schlick(float n_dot_v,
                                                         float roughness) {
  const float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
  return n_dot_v / (n_dot_v * (1.0f - k) + k);
}

// Light occlusion consists of two phenomena:
// - shadowing (incoming light blocked): G(v)
// - masking (reflected light blocked from reaching the camera): G(l)
// Multiplying them to compute the final geometric attenuation (G(v, l)).
RAYPALETTE_HOST_DEVICE inline float ggx_geometry(float n_dot_v,
                                                 float n_dot_l,
                                                 float roughness) {
  return ggx_geometry_schlick(n_dot_v, roughness) *
         ggx_geometry_schlick(n_dot_l, roughness);
}

RAYPALETTE_HOST_DEVICE inline bool is_valid_material(const Material &material) {
    if (!is_unit_color(material.base_color) || !is_unit_color(material.emission_color) ||
      !is_unit_color(material.transmission_color) ||
      material.roughness < 0.0f || material.roughness > 1.0f ||
      material.emission_strength < 0.0f || material.absorption_density < 0.0f) {
    return false;
  }

    return material.type != MaterialType::Dielectric ||
      material.index_of_refraction > 1.0f;
}

} // namespace raypalette
