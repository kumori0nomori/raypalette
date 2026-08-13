#pragma once

#include "render/ray.hpp"

namespace raypalette {

struct Camera {
  Vec3 origin{0.0f, 1.5f, 5.0f};
  Vec3 lower_left_corner{-1.0f, 0.5f, 0.0f};
  Vec3 horizontal{2.0f, 0.0f, 0.0f};
  Vec3 vertical{0.0f, 1.0f, 0.0f};
};

inline Camera make_default_camera(float aspect_ratio,
                                  float camera_distance = 5.0f) {
  const Vec3 origin{0.0f, 1.5f, camera_distance};
  const Vec3 look_at{0.0f, 1.0f, 0.0f};
  const Vec3 up{0.0f, 1.0f, 0.0f};
  const float viewport_height = 2.0f;
  const float viewport_width = viewport_height * aspect_ratio;
  const Vec3 forward = normalized(look_at - origin);
  const Vec3 right = normalized(cross(forward, up));
  const Vec3 camera_up = cross(right, forward);
  const Vec3 horizontal = viewport_width * right;
  const Vec3 vertical = viewport_height * camera_up;
  const Vec3 lower_left_corner = origin + forward - horizontal * 0.5f - vertical * 0.5f;

  return {origin, lower_left_corner, horizontal, vertical};
}

RAYPALETTE_HOST_DEVICE inline Ray camera_ray(const Camera &camera,
                                             float u,
                                             float v) {
  return {camera.origin,
          camera.lower_left_corner + u * camera.horizontal + v * camera.vertical - camera.origin};
}

} // namespace raypalette
