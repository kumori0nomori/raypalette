#include "render/scene.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace raypalette {
namespace {

TEST(Material, AcceptsDefaultDiffuseMaterial) {
  EXPECT_TRUE(is_valid_material({}));
}

TEST(Material, RejectsInvalidPhysicalParameters) {
  Material invalid_color;
  invalid_color.base_color = {1.1f, 0.0f, 0.0f};
  EXPECT_FALSE(is_valid_material(invalid_color));

  Material invalid_roughness;
  invalid_roughness.roughness = -0.1f;
  EXPECT_FALSE(is_valid_material(invalid_roughness));

  Material invalid_ior;
  invalid_ior.type = MaterialType::Dielectric;
  invalid_ior.index_of_refraction = 0.9f;
  EXPECT_FALSE(is_valid_material(invalid_ior));

  Material invalid_emission;
  invalid_emission.emission_strength = -1.0f;
  EXPECT_FALSE(is_valid_material(invalid_emission));
}

TEST(PointLight, ConvertsPolarPositionAroundSphereCenter) {
  const Vec3 sphere_center{0.0f, 1.0f, 0.0f};
  const PointLight light = point_light_from_polar({2.0f, 0.0f, 0.0f},
                                                   sphere_center,
                                                   {1.0f, 0.5f, 0.25f}, 50.0f);

  EXPECT_NEAR(light.position.x, 0.0f, 1.0e-6f);
  EXPECT_NEAR(light.position.y, 3.0f, 1.0e-6f);
  EXPECT_NEAR(light.position.z, 0.0f, 1.0e-6f);
  EXPECT_TRUE(is_valid_point_light(light));
}

TEST(PointLight, RejectsInvalidColorAndIntensity) {
  PointLight invalid_color;
  invalid_color.color = {-0.1f, 1.0f, 1.0f};
  EXPECT_FALSE(is_valid_point_light(invalid_color));

  PointLight invalid_intensity;
  invalid_intensity.intensity = -1.0f;
  EXPECT_FALSE(is_valid_point_light(invalid_intensity));

  PointLight invalid_position;
  invalid_position.position.x = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(is_valid_point_light(invalid_position));
}

TEST(Scene, CreatesCanonicalSphereAndFloor) {
  const Scene scene = make_default_scene();

  EXPECT_FLOAT_EQ(scene.sphere.center.x, 0.0f);
  EXPECT_FLOAT_EQ(scene.sphere.center.y, 1.0f);
  EXPECT_FLOAT_EQ(scene.sphere.radius, 1.0f);
  EXPECT_FLOAT_EQ(scene.floor.point.y, 0.0f);
  EXPECT_FLOAT_EQ(scene.floor.normal.y, 1.0f);
  EXPECT_EQ(scene.sphere.material_index, kSphereMaterialIndex);
  EXPECT_EQ(scene.floor.material_index, kFloorMaterialIndex);
  EXPECT_TRUE(is_valid_material(scene.materials[kSphereMaterialIndex]));
  EXPECT_TRUE(is_valid_material(scene.materials[kFloorMaterialIndex]));
  EXPECT_TRUE(is_valid_point_light(scene.point_light));
}

} // namespace
} // namespace raypalette