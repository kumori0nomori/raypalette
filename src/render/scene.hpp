#pragma once

#include "render/geometry.hpp"
#include "render/light.hpp"
#include "render/material.hpp"

namespace raypalette {

constexpr std::uint32_t kSphereMaterialIndex = 0;
constexpr std::uint32_t kFloorMaterialIndex = 1;

struct EnvironmentLight {
  Vec3 color{1.0f, 1.0f, 1.0f};
  float intensity = 0.08f;
};

struct Scene {
  Material materials[2];
  Sphere sphere;
  Plane floor;
  Light light;
  EnvironmentLight environment;
  Vec3 background_color{0.05f, 0.05f, 0.05f};
};

RAYPALETTE_HOST_DEVICE inline bool is_valid_scene(const Scene &scene) {
  return is_valid_material(scene.materials[kSphereMaterialIndex]) &&
         is_valid_material(scene.materials[kFloorMaterialIndex]) &&
         scene.sphere.radius > 0.0f && is_finite(scene.sphere.center) &&
         is_finite(scene.floor.point) &&
         length_squared(scene.floor.normal) > 1.0e-12f &&
         is_valid_light(scene.light) && is_unit_color(scene.environment.color) &&
         scene.environment.intensity >= 0.0f &&
         is_nonnegative_color(scene.background_color);
}

inline Scene make_default_scene() {
  const Vec3 sphere_center{0.0f, 1.0f, 0.0f};
  const PolarCoordinates light_polar{4.0f, 35.0f, 45.0f};

  return {{{MaterialType::Diffuse, {0.8f, 0.2f, 0.1f}, 0.5f, 1.5f, {}, 0.0f},
           {MaterialType::Diffuse, {0.5f, 0.5f, 0.5f}, 0.5f, 1.5f, {}, 0.0f}},
          {sphere_center, 1.0f, kSphereMaterialIndex},
          {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, kFloorMaterialIndex},
          make_point_light(light_polar, sphere_center, {1.0f, 1.0f, 1.0f}, 100.0f),
          {{1.0f, 1.0f, 1.0f}, 0.08f},
          {0.05f, 0.05f, 0.05f}};
}

} // namespace raypalette
