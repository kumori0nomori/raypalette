#include "render/geometry.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace raypalette {
namespace {

constexpr float kMinimumDistance = 0.001f;
constexpr float kMaximumDistance = std::numeric_limits<float>::infinity();

TEST(SphereIntersection, ReturnsNearestFrontFaceHit) {
  const Sphere sphere{{0.0f, 0.0f, 0.0f}, 1.0f, 7};
  const Ray ray{{0.0f, 0.0f, -3.0f}, {0.0f, 0.0f, 1.0f}};
  HitRecord record;

  ASSERT_TRUE(hit_sphere(sphere, ray, kMinimumDistance, kMaximumDistance, record));
  EXPECT_NEAR(record.distance, 2.0f, 1.0e-6f);
  EXPECT_NEAR(record.position.z, -1.0f, 1.0e-6f);
  EXPECT_FLOAT_EQ(record.normal.z, -1.0f);
  EXPECT_TRUE(record.front_face);
  EXPECT_EQ(record.material_index, 7U);
}

TEST(SphereIntersection, FlipsNormalForInsideHit) {
  const Sphere sphere{{0.0f, 0.0f, 0.0f}, 1.0f, 0};
  const Ray ray{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
  HitRecord record;

  ASSERT_TRUE(hit_sphere(sphere, ray, kMinimumDistance, kMaximumDistance, record));
  EXPECT_NEAR(record.distance, 1.0f, 1.0e-6f);
  EXPECT_FLOAT_EQ(record.normal.z, -1.0f);
  EXPECT_FALSE(record.front_face);
}

TEST(SphereIntersection, RejectsMissAndInvalidRadius) {
  const Ray ray{{0.0f, 0.0f, -3.0f}, {0.0f, 0.0f, 1.0f}};
  HitRecord record;

  EXPECT_FALSE(
      hit_sphere({{3.0f, 0.0f, 0.0f}, 1.0f, 0}, ray, kMinimumDistance, kMaximumDistance, record));
  EXPECT_FALSE(
      hit_sphere({{0.0f, 0.0f, 0.0f}, 0.0f, 0}, ray, kMinimumDistance, kMaximumDistance, record));
}

TEST(PlaneIntersection, ReturnsFloorHitAndMaterial) {
  const Plane floor{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 3};
  const Ray ray{{0.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}};
  HitRecord record;

  ASSERT_TRUE(hit_plane(floor, ray, kMinimumDistance, kMaximumDistance, record));
  EXPECT_NEAR(record.distance, 2.0f, 1.0e-6f);
  EXPECT_NEAR(record.position.y, 0.0f, 1.0e-6f);
  EXPECT_FLOAT_EQ(record.normal.y, 1.0f);
  EXPECT_EQ(record.material_index, 3U);
}

TEST(PlaneIntersection, RejectsParallelRaysAndZeroNormal) {
  const Plane floor{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 0};
  const Ray parallel_ray{{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
  HitRecord record;

  EXPECT_FALSE(hit_plane(floor, parallel_ray, kMinimumDistance, kMaximumDistance, record));
  EXPECT_FALSE(hit_plane({{0.0f, 0.0f, 0.0f}, {}, 0}, parallel_ray, kMinimumDistance,
                         kMaximumDistance, record));
}

} // namespace
} // namespace raypalette