#pragma once

#include "math/color.hpp"
#include "math/vec3.hpp"

namespace raypalette {

constexpr float kPi = 3.14159265358979323846f;

//
// Compute the reflected direction and the refracted direction for a dielectric material.
// - Reflection: light reflects off an object's surface like a mirror. (metal)
// - Refraction: light bends as it passes through a transparent material. (glass)
//

RAYPALETTE_HOST_DEVICE constexpr Vec3 reflect_direction(const Vec3& incoming, const Vec3& normal) {
  return incoming - 2.0f * dot(incoming, normal) * normal;
}

RAYPALETTE_HOST_DEVICE inline bool refract_direction(const Vec3& incoming, const Vec3& normal,
                                                     float eta_ratio, Vec3& refracted) {
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
RAYPALETTE_HOST_DEVICE inline Vec3
beer_lambert_attenuation(const Vec3& transmission_color, float absorption_density, float distance) {
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
// where R0 = ((1- eta_ratio) / (1 + eta_ratio))^2.
RAYPALETTE_HOST_DEVICE inline float schlick_reflectance(float cosine, float eta_ratio) {
  float reflectance = (1.0f - eta_ratio) / (1.0f + eta_ratio);
  reflectance *= reflectance;
  return reflectance + (1.0f - reflectance) * powf(1.0f - cosine, 5.0f);
}

// Metallic Fresnel reflectance using an RGB base reflectance (F0).
// Returns a colored reflection factor for the GGX BRDF.
// F = F0 + (1 - F0) * (1 - cos(theta))^5
// Reference:
// - https://ja.wikipedia.org/wiki/%E3%83%95%E3%83%AC%E3%83%8D%E3%83%ルの式
RAYPALETTE_HOST_DEVICE inline Vec3 schlick_fresnel(const Vec3& f0, float cosine) {
  const float factor = powf(1.0f - fmaxf(0.0f, fminf(1.0f, cosine)), 5.0f);
  return f0 + (Vec3{1.0f, 1.0f, 1.0f} - f0) * factor;
}

// GGX microfacet distribution function. (Trowbridge-Reitz (GGX) NDF)
// D(h) = alpha^2 / (pi * ((n.h)^2 * (alpha^2 - 1) + 1)^2)
// Reference:
// - https://hanecci.hatenadiary.org/entry/20130511/p1
RAYPALETTE_HOST_DEVICE inline float ggx_distribution(float n_dot_h, float roughness) {
  const float alpha = fmaxf(0.001f, roughness * roughness);
  const float alpha_squared = alpha * alpha;
  const float n_dot_h_squared = n_dot_h * n_dot_h;
  const float denominator = n_dot_h_squared * (alpha_squared - 1.0f) + 1.0f;
  return alpha_squared / (kPi * denominator * denominator);
}

RAYPALETTE_HOST_DEVICE inline void ggx_anisotropic_alphas(float roughness, float anisotropy,
                                                          float& alpha_x, float& alpha_y) {
  const float aspect = sqrtf(fmaxf(0.1f, 1.0f - 0.9f * fabsf(anisotropy)));
  const float alpha = fmaxf(0.001f, roughness * roughness);
  alpha_x = alpha / aspect;
  alpha_y = alpha * aspect;
}

RAYPALETTE_HOST_DEVICE inline float
ggx_anisotropic_distribution(const Vec3& normal, const Vec3& tangent, const Vec3& half_vector,
                             float roughness, float anisotropy) {
  if (fabsf(anisotropy) <= 1.0e-6f) {
    return ggx_distribution(dot(normal, half_vector), roughness);
  }
  const Vec3 bitangent = cross(normal, tangent);
  const float h_tangent = dot(half_vector, tangent);
  const float h_bitangent = dot(half_vector, bitangent);
  const float h_normal = fmaxf(0.0f, dot(half_vector, normal));
  float alpha_x;
  float alpha_y;
  ggx_anisotropic_alphas(roughness, anisotropy, alpha_x, alpha_y);
  const float scaled_squared = (h_tangent * h_tangent) / (alpha_x * alpha_x) +
                               (h_bitangent * h_bitangent) / (alpha_y * alpha_y) +
                               h_normal * h_normal;
  return 1.0f / fmaxf(1.0e-12f, kPi * alpha_x * alpha_y * scaled_squared * scaled_squared);
}

// Approximating shadowing and masking by microfacets. (Schlick-GGX model)
// G(v) = n.v / (n.v * (1 - k) + k)
// where k = (alpha + 1)^2 / 8
RAYPALETTE_HOST_DEVICE inline float ggx_geometry_schlick(float n_dot_v, float roughness) {
  const float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
  return n_dot_v / (n_dot_v * (1.0f - k) + k);
}

// Light occlusion consists of two phenomena:
// - shadowing (incoming light blocked): G(v)
// - masking (reflected light blocked from reaching the camera): G(l)
// Multiplying them to compute the final geometric attenuation (G(v, l)).
RAYPALETTE_HOST_DEVICE inline float ggx_geometry(float n_dot_v, float n_dot_l, float roughness) {
  return ggx_geometry_schlick(n_dot_v, roughness) * ggx_geometry_schlick(n_dot_l, roughness);
}

RAYPALETTE_HOST_DEVICE inline float ggx_reflection_pdf(float n_dot_h, float v_dot_h,
                                                       float roughness) {
  if (n_dot_h <= 0.0f || v_dot_h <= 1.0e-6f) {
    return 0.0f;
  }
  return ggx_distribution(n_dot_h, roughness) * n_dot_h / (4.0f * v_dot_h);
}

RAYPALETTE_HOST_DEVICE inline float
ggx_anisotropic_reflection_pdf(const Vec3& normal, const Vec3& tangent, const Vec3& half_vector,
                               float v_dot_h, float roughness, float anisotropy) {
  const float n_dot_h = fmaxf(0.0f, dot(normal, half_vector));
  if (n_dot_h <= 0.0f || v_dot_h <= 1.0e-6f) {
    return 0.0f;
  }
  return ggx_anisotropic_distribution(normal, tangent, half_vector, roughness, anisotropy) *
         n_dot_h / (4.0f * v_dot_h);
}

} // namespace raypalette
