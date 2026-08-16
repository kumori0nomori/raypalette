#pragma once

#include "render/ray.hpp"

#include <cstdint>

namespace raypalette {

RAYPALETTE_HOST_DEVICE inline Vec3 stable_tangent(const Vec3& normal) {
  const Vec3 reference = fabsf(normal.y) < 0.9f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
  return normalized(cross(reference, normal));
}

struct HitRecord {
  Vec3 position;
  Vec3 normal;
  Vec3 tangent;
  float distance = 0.0f;
  std::uint32_t material_index = 0;
  bool front_face = true;

  RAYPALETTE_HOST_DEVICE void set_face_normal(const Ray& ray, const Vec3& outward_normal) {
    front_face = dot(ray.direction, outward_normal) < 0.0f;
    normal = front_face ? outward_normal : -outward_normal;
  }

  RAYPALETTE_HOST_DEVICE void set_tangent() {
    tangent = stable_tangent(normal);
  }
};

struct Sphere {
  Vec3 center;
  float radius = 1.0f;
  std::uint32_t material_index = 0;
};

struct Plane {
  Vec3 point;
  Vec3 normal{0.0f, 1.0f, 0.0f};
  std::uint32_t material_index = 0;
};

RAYPALETTE_HOST_DEVICE inline bool hit_sphere(const Sphere& sphere, const Ray& ray,
                                              float minimum_distance, float maximum_distance,
                                              HitRecord& record) {
  if (sphere.radius <= 0.0f) {
    return false;
  }

  const Vec3 center_to_origin = ray.origin - sphere.center;
  const float direction_length_squared = length_squared(ray.direction);
  if (direction_length_squared <= 0.0f) {
    return false;
  }

  const float half_b = dot(center_to_origin, ray.direction);
  const float c = length_squared(center_to_origin) - sphere.radius * sphere.radius;
  const float discriminant = half_b * half_b - direction_length_squared * c;
  if (discriminant < 0.0f) {
    return false;
  }

  const float root_offset = sqrtf(discriminant);
  float distance = (-half_b - root_offset) / direction_length_squared;
  if (distance <= minimum_distance || distance >= maximum_distance) {
    distance = (-half_b + root_offset) / direction_length_squared;
    if (distance <= minimum_distance || distance >= maximum_distance) {
      return false;
    }
  }

  record.distance = distance;
  record.position = ray.at(distance);
  record.material_index = sphere.material_index;
  const Vec3 outward_normal = (record.position - sphere.center) * (1.0f / sphere.radius);
  record.set_face_normal(ray, outward_normal);
  record.set_tangent();
  return true;
}

RAYPALETTE_HOST_DEVICE inline bool hit_plane(const Plane& plane, const Ray& ray,
                                             float minimum_distance, float maximum_distance,
                                             HitRecord& record) {
  constexpr float parallel_threshold = 1.0e-6f;
  const Vec3 plane_normal = normalized(plane.normal);
  if (length_squared(plane_normal) == 0.0f) {
    return false;
  }

  const float denominator = dot(plane_normal, ray.direction);
  if (fabsf(denominator) <= parallel_threshold) {
    return false;
  }

  const float distance = dot(plane.point - ray.origin, plane_normal) / denominator;
  if (distance <= minimum_distance || distance >= maximum_distance) {
    return false;
  }

  record.distance = distance;
  record.position = ray.at(distance);
  record.material_index = plane.material_index;
  record.set_face_normal(ray, plane_normal);
  record.set_tangent();
  return true;
}

} // namespace raypalette
