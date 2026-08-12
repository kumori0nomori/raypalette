#pragma once

#include <cmath>

#if defined(__CUDACC__)
#define RAYPALETTE_HOST_DEVICE __host__ __device__
#else
#define RAYPALETTE_HOST_DEVICE
#endif

namespace raypalette {

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  RAYPALETTE_HOST_DEVICE constexpr Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
  RAYPALETTE_HOST_DEVICE constexpr Vec3(float x_value, float y_value,
                                        float z_value)
      : x(x_value), y(y_value), z(z_value) {}

  RAYPALETTE_HOST_DEVICE constexpr Vec3 &operator+=(const Vec3 &other) {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
  }

  RAYPALETTE_HOST_DEVICE constexpr Vec3 &operator-=(const Vec3 &other) {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
  }

  RAYPALETTE_HOST_DEVICE constexpr Vec3 &operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
  }
};

RAYPALETTE_HOST_DEVICE constexpr Vec3 operator+(Vec3 left, const Vec3 &right) {
  return left += right;
}

RAYPALETTE_HOST_DEVICE constexpr Vec3 operator-(Vec3 left, const Vec3 &right) {
  return left -= right;
}

RAYPALETTE_HOST_DEVICE constexpr Vec3 operator-(const Vec3 &value) {
  return {-value.x, -value.y, -value.z};
}

RAYPALETTE_HOST_DEVICE constexpr Vec3 operator*(Vec3 value, float scalar) {
  return value *= scalar;
}

RAYPALETTE_HOST_DEVICE constexpr Vec3 operator*(float scalar, Vec3 value) {
  return value *= scalar;
}

RAYPALETTE_HOST_DEVICE constexpr Vec3 operator*(const Vec3 &left,
                                                const Vec3 &right) {
  return {left.x * right.x, left.y * right.y, left.z * right.z};
}

RAYPALETTE_HOST_DEVICE constexpr float dot(const Vec3 &left, const Vec3 &right) {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

RAYPALETTE_HOST_DEVICE constexpr Vec3 cross(const Vec3 &left,
                                            const Vec3 &right) {
  return {left.y * right.z - left.z * right.y,
          left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

RAYPALETTE_HOST_DEVICE constexpr float length_squared(const Vec3 &value) {
  return dot(value, value);
}

RAYPALETTE_HOST_DEVICE inline float length(const Vec3 &value) {
  return sqrtf(length_squared(value));
}

RAYPALETTE_HOST_DEVICE inline Vec3 normalized(const Vec3 &value) {
  constexpr float minimum_length_squared = 1.0e-12f;
  const float value_length_squared = length_squared(value);
  if (value_length_squared <= minimum_length_squared) {
    return {};
  }
  return value * (1.0f / sqrtf(value_length_squared));
}

RAYPALETTE_HOST_DEVICE inline bool is_finite(const Vec3 &value) {
#if defined(__CUDACC__)
  return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
#else
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
#endif
}

} // namespace raypalette
