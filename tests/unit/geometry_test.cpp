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
  EXPECT_NEAR(length(record.tangent), 1.0f, 1.0e-6f);
  EXPECT_NEAR(dot(record.normal, record.tangent), 0.0f, 1.0e-6f);
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
  EXPECT_NEAR(length(record.tangent), 1.0f, 1.0e-6f);
  EXPECT_NEAR(dot(record.normal, record.tangent), 0.0f, 1.0e-6f);
}

TEST(Geometry, GeneratesStableOrthogonalTangent) {
  const Vec3 normals[] = {{1.0f, 0.0f, 0.0f},
                          {0.0f, 1.0f, 0.0f},
                          {0.0f, 0.0f, 1.0f},
                          normalized(Vec3{1.0f, 2.0f, 3.0f})};
  for (const Vec3& normal : normals) {
    const Vec3 tangent = stable_tangent(normal);
    EXPECT_TRUE(is_finite(tangent));
    EXPECT_NEAR(length(tangent), 1.0f, 1.0e-6f);
    EXPECT_NEAR(dot(normal, tangent), 0.0f, 1.0e-6f);
  }
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