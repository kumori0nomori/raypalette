#include "math/color.hpp"
#include "math/polar.hpp"
#include "render/geometry.hpp"
#include "render/light.hpp"
#include "render/material.hpp"

namespace raypalette {

__global__ void math_compile_check_kernel(Vec3 *output) {
  const Vec3 direction = normalized(polar_to_cartesian(1.0f, 90.0f, 0.0f));
  output[0] = linear_to_srgb(apply_exposure(direction, 0.0f));

  const Sphere sphere{{0.0f, 1.0f, 0.0f}, 1.0f, 0};
  const Plane floor{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 1};
  const Ray ray{{0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}};
  HitRecord record;
  hit_sphere(sphere, ray, 0.001f, 1000.0f, record);
  hit_plane(floor, ray, 0.001f, 1000.0f, record);

  const Material material{};
  const PointLight light = point_light_from_polar({2.0f, 45.0f, 45.0f},
                                                  sphere.center, {1.0f, 1.0f, 1.0f},
                                                  100.0f);
  if (!is_valid_material(material) || !is_valid_point_light(light)) {
    output[0] = {};
  }
}

} // namespace raypalette