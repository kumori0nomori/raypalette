#include "ui/hsv_plot.hpp"

#include <algorithm>
#include <cmath>

namespace raypalette::ui {

std::vector<HsvPlotPoint> make_sphere_hsv_points(const Texture& texture, const Scene& scene,
                                                 const Camera& camera,
                                                 const RenderSettings& settings) {
  constexpr int kMaximumPointCount = 2000;
  std::vector<HsvPlotPoint> points;
  if (texture.width <= 0 || texture.height <= 0 || texture.display_pixels.empty()) {
    return points;
  }

  const int pixel_count = texture.width * texture.height;
  const int stride = std::max(
      1,
      static_cast<int>(std::ceil(std::sqrt(static_cast<float>(pixel_count) / kMaximumPointCount))));
  points.reserve(std::min(kMaximumPointCount, pixel_count));
  for (int pixel_y = stride / 2; pixel_y < texture.height; pixel_y += stride) {
    for (int pixel_x = stride / 2; pixel_x < texture.width; pixel_x += stride) {
      const float u = (static_cast<float>(pixel_x) + 0.5f) / texture.width;
      const float v = (static_cast<float>(pixel_y) + 0.5f) / texture.height;
      HitRecord hit;
      if (!hit_sphere(scene.sphere, camera_ray(camera, u, v), settings.minimum_distance, 1.0e30f,
                      hit)) {
        continue;
      }
      const Vec3 display_color = texture.display_pixels[pixel_y * texture.width + pixel_x];
      const Hsv hsv = srgb_to_hsv(display_color);
      points.push_back({display_color, hsv, hsv_cylinder_position(hsv)});
    }
  }
  return points;
}

Vec3 rotate_hsv_plot_position(const Vec3& position, const HsvPlotView& view) {
  const float yaw_cosine = std::cos(view.yaw);
  const float yaw_sine = std::sin(view.yaw);
  const float pitch_cosine = std::cos(view.pitch);
  const float pitch_sine = std::sin(view.pitch);
  const float yaw_x = yaw_cosine * position.x - yaw_sine * position.z;
  const float yaw_z = yaw_sine * position.x + yaw_cosine * position.z;
  return {yaw_x, pitch_cosine * (position.y - 0.5f) - pitch_sine * yaw_z,
          pitch_sine * (position.y - 0.5f) + pitch_cosine * yaw_z};
}

Vec3 cylinder_position_at_hue(float hue, float saturation, float value) {
  constexpr float kTwoPi = 6.28318530717958647692f;
  const float angle = kTwoPi * hue;
  return {saturation * std::cos(angle), value, saturation * std::sin(angle)};
}

float hue_distance(float first, float second) {
  const float direct_distance = std::fabs(first - second);
  return std::min(direct_distance, 1.0f - direct_distance);
}

} // namespace raypalette::ui
