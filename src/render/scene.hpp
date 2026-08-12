#pragma once

#include "render/geometry.hpp"
#include "render/light.hpp"
#include "render/material.hpp"

namespace raypalette {

constexpr std::uint32_t kSphereMaterialIndex = 0;
constexpr std::uint32_t kFloorMaterialIndex = 1;

struct Scene {
  Material materials[2];
  Sphere sphere;
  Plane floor;
  PointLight point_light;
};

inline Scene make_default_scene() {
  const Vec3 sphere_center{0.0f, 1.0f, 0.0f};
  const PolarCoordinates light_polar{4.0f, 35.0f, 45.0f};

  return {{{MaterialType::Diffuse, {0.8f, 0.2f, 0.1f}, 0.5f, 1.5f, {}, 0.0f},
           {MaterialType::Diffuse, {0.5f, 0.5f, 0.5f}, 0.5f, 1.5f, {}, 0.0f}},
          {sphere_center, 1.0f, kSphereMaterialIndex},
          {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, kFloorMaterialIndex},
          point_light_from_polar(light_polar, sphere_center, {1.0f, 1.0f, 1.0f},
                                 100.0f)};
}

} // namespace raypalette
