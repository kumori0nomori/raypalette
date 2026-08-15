#include "render/bsdf.hpp"
#include "render/light_sampling.hpp"
#include "render/scene.hpp"
#include "render/tracer.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace raypalette {
namespace {

TEST(Material, AcceptsDefaultSurfaceMaterial) {
  EXPECT_TRUE(is_valid_material({}));
}

TEST(Material, ResolvesSurfacePresets) {
  Material material;
  apply_material_preset(material, MaterialPreset::Metal);
  material.roughness = 0.2f;
  PrincipledParameters metal = resolve_principled_parameters(material);
  EXPECT_FLOAT_EQ(metal.metallic, 1.0f);
  EXPECT_FLOAT_EQ(metal.roughness, 0.2f);

  apply_material_preset(material, MaterialPreset::Glossy);
  const PrincipledParameters glossy = resolve_principled_parameters(material);
  EXPECT_FLOAT_EQ(glossy.metallic, 0.0f);
  EXPECT_FLOAT_EQ(glossy.roughness, 0.22f);
  EXPECT_FLOAT_EQ(glossy.coat, 0.7f);
  EXPECT_FLOAT_EQ(glossy.coat_roughness, 0.04f);

  apply_material_preset(material, MaterialPreset::Cloth);
  const PrincipledParameters cloth = resolve_principled_parameters(material);
  EXPECT_FLOAT_EQ(cloth.sheen, 0.6f);
}

TEST(Material, UsesConsistentSurfaceMixturePdf) {
  Material surface;
  const float diffuse_pdf = 0.25f;
  const float specular_pdf = 0.75f;
  const float mixture_pdf = detail::surface_mixture_pdf(surface, diffuse_pdf, specular_pdf);
  EXPECT_GT(detail::surface_specular_probability(surface), 0.0f);
  EXPECT_LT(detail::surface_specular_probability(surface), 1.0f);
  EXPECT_GT(mixture_pdf, diffuse_pdf);
  EXPECT_LT(mixture_pdf, specular_pdf);
  EXPECT_TRUE(std::isfinite(mixture_pdf));

  apply_material_preset(surface, MaterialPreset::Metal);
  EXPECT_FLOAT_EQ(detail::surface_specular_probability(surface), 1.0f);
  EXPECT_FLOAT_EQ(detail::surface_mixture_pdf(surface, diffuse_pdf, specular_pdf), specular_pdf);
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

TEST(Material, EvaluatesFiniteGgxTerms) {
  const Vec3 f0{0.8f, 0.6f, 0.4f};
  const Vec3 grazing = schlick_fresnel(f0, 0.0f);
  const Vec3 normal = schlick_fresnel(f0, 1.0f);

  EXPECT_NEAR(normal.x, f0.x, 1.0e-6f);
  EXPECT_NEAR(normal.y, f0.y, 1.0e-6f);
  EXPECT_NEAR(grazing.x, 1.0f, 1.0e-6f);
  EXPECT_NEAR(grazing.y, 1.0f, 1.0e-6f);
  EXPECT_TRUE(std::isfinite(ggx_distribution(0.5f, 0.0f)));
  EXPECT_TRUE(std::isfinite(ggx_distribution(0.5f, 1.0f)));
}

TEST(Material, ComputesRefractionAndFresnel) {
  Vec3 refracted;
  EXPECT_TRUE(refract_direction({0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 1.0f / 1.5f, refracted));
  EXPECT_NEAR(refracted.y, -1.0f, 1.0e-6f);
  EXPECT_NEAR(schlick_reflectance(1.0f, 1.0f / 1.5f), 0.04f, 1.0e-6f);
  EXPECT_NEAR(schlick_reflectance(1.0f, 1.5f), 0.04f, 1.0e-6f);
  EXPECT_FALSE(refract_direction({0.8660254f, 0.0f, 0.5f}, {0.0f, 0.0f, -1.0f}, 1.5f, refracted));
}

TEST(Material, AppliesBeerLambertAbsorption) {
  const Vec3 clear = beer_lambert_attenuation({1.0f, 1.0f, 1.0f}, 2.0f, 3.0f);
  const Vec3 tinted = beer_lambert_attenuation({1.0f, 0.5f, 0.25f}, 1.0f, 2.0f);

  EXPECT_FLOAT_EQ(clear.x, 1.0f);
  EXPECT_FLOAT_EQ(clear.y, 1.0f);
  EXPECT_FLOAT_EQ(clear.z, 1.0f);
  EXPECT_NEAR(tinted.x, 1.0f, 1.0e-6f);
  EXPECT_NEAR(tinted.y, 0.25f, 1.0e-6f);
  EXPECT_NEAR(tinted.z, 0.0625f, 1.0e-6f);
}

TEST(Material, ColoredGlassAttenuatesChannelsIndependently) {
  const Vec3 attenuation = beer_lambert_attenuation({0.8f, 0.4f, 0.2f}, 1.0f, 2.0f);

  EXPECT_NEAR(attenuation.x, 0.64f, 1.0e-5f);
  EXPECT_NEAR(attenuation.y, 0.16f, 1.0e-5f);
  EXPECT_NEAR(attenuation.z, 0.04f, 1.0e-5f);
  EXPECT_GT(attenuation.x, attenuation.y);
  EXPECT_GT(attenuation.y, attenuation.z);
}

TEST(Material, RoughnessChangesGgxDistribution) {
  const float smooth = ggx_distribution(1.0f, 0.1f);
  const float rough = ggx_distribution(1.0f, 0.9f);

  EXPECT_GT(smooth, rough);
}

TEST(Light, ConvertsPointLightPolarPositionAroundSphereCenter) {
  const Vec3 sphere_center{0.0f, 1.0f, 0.0f};
  const Light light =
      make_point_light({2.0f, 0.0f, 0.0f}, sphere_center, {1.0f, 0.5f, 0.25f}, 50.0f);

  EXPECT_EQ(light.type, LightType::Point);
  EXPECT_NEAR(light.point.position.x, 0.0f, 1.0e-6f);
  EXPECT_NEAR(light.point.position.y, 3.0f, 1.0e-6f);
  EXPECT_NEAR(light.point.position.z, 0.0f, 1.0e-6f);
  EXPECT_TRUE(is_valid_light(light));
}

TEST(Light, RejectsInvalidColorAndIntensity) {
  Light invalid_color;
  invalid_color.point.color = {-0.1f, 1.0f, 1.0f};
  EXPECT_FALSE(is_valid_light(invalid_color));

  Light invalid_intensity;
  invalid_intensity.point.radiant_intensity = -1.0f;
  EXPECT_FALSE(is_valid_light(invalid_intensity));

  Light invalid_position;
  invalid_position.point.position.x = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(is_valid_light(invalid_position));
}

TEST(Light, SamplesPointLightDirectionAndInverseSquareRadiance) {
  Light light;
  light.point.position = {0.0f, 4.0f, 0.0f};
  light.point.color = {1.0f, 0.5f, 0.25f};
  light.point.radiant_intensity = 16.0f;
  LightSample near_sample;
  LightSample far_sample;

  ASSERT_TRUE(sample_light(light, {0.0f, 2.0f, 0.0f}, near_sample));
  ASSERT_TRUE(sample_light(light, {0.0f, 0.0f, 0.0f}, far_sample));

  EXPECT_FLOAT_EQ(near_sample.direction_to_light.x, 0.0f);
  EXPECT_FLOAT_EQ(near_sample.direction_to_light.y, 1.0f);
  EXPECT_FLOAT_EQ(near_sample.distance, 2.0f);
  EXPECT_FLOAT_EQ(near_sample.radiance.x, 4.0f);
  EXPECT_FLOAT_EQ(near_sample.radiance.y, 2.0f);
  EXPECT_FLOAT_EQ(near_sample.pdf, 0.0f);
  EXPECT_NEAR(far_sample.radiance.x, near_sample.radiance.x * 0.25f, 1.0e-6f);
}

TEST(Light, RejectsSurfaceAtPointLightPosition) {
  Light light;
  light.point.position = {1.0f, 2.0f, 3.0f};
  LightSample sample;

  EXPECT_FALSE(sample_light(light, light.point.position, sample));
}

TEST(Light, CreatesDirectionalLightFromPolarDirection) {
  const Light light = make_directional_light(90.0f, 0.0f, {1.0f, 0.5f, 0.25f}, 3.0f);
  LightSample sample;

  ASSERT_TRUE(sample_light(light, {12.0f, -3.0f, 8.0f}, sample));
  EXPECT_EQ(light.type, LightType::Directional);
  EXPECT_NEAR(length(light.directional.direction_to_light), 1.0f, 1.0e-6f);
  EXPECT_NEAR(sample.direction_to_light.x, 1.0f, 1.0e-6f);
  EXPECT_NEAR(sample.direction_to_light.y, 0.0f, 1.0e-6f);
  EXPECT_NEAR(sample.radiance.x, 3.0f, 1.0e-6f);
  EXPECT_GT(sample.distance, 1.0e20f);
}

TEST(Light, ValidatesRectAreaParametersWithoutSampling) {
  const Light light =
      make_rect_area_light({3.0f, 45.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 2.0f, 1.0f,
                           {1.0f, 1.0f, 1.0f}, 5.0f);
  LightSample sample;

  EXPECT_EQ(light.type, LightType::RectArea);
  EXPECT_TRUE(is_valid_light(light));
  EXPECT_FALSE(sample_light(light, {}, sample));
  ASSERT_TRUE(sample_area_light(light, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f, sample));
  EXPECT_NEAR(sample.direction_to_light.y, 0.8891311f, 1.0e-6f);
  EXPECT_GT(sample.radiance.x, 0.0f);
  EXPECT_GT(sample.pdf, 0.0f);

  EXPECT_FALSE(sample_area_light(light, {0.0f, 10.0f, 0.0f}, 0.0f, 0.0f, sample));
  EXPECT_FALSE(sample_area_light(light, {0.0f, -1.0f, 0.0f}, 0.6f, 0.0f, sample));

  Light invalid_light = light;
  invalid_light.area.width = 0.0f;
  EXPECT_FALSE(is_valid_light(invalid_light));
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
  EXPECT_EQ(scene.light.type, LightType::Point);
  EXPECT_TRUE(is_valid_light(scene.light));
  EXPECT_TRUE(is_unit_color(scene.environment.color));
  EXPECT_FLOAT_EQ(scene.environment.intensity, 0.08f);
  EXPECT_TRUE(is_valid_scene(scene));
}

TEST(Scene, ValidatesEnvironmentRadianceValues) {
  Scene scene = make_default_scene();
  scene.environment.color = {1.0f, 0.5f, 0.0f};
  EXPECT_TRUE(is_valid_scene(scene));

  scene.environment.color = {-0.01f, 0.0f, 0.0f};
  EXPECT_FALSE(is_valid_scene(scene));

  scene.environment.color.x = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(is_valid_scene(scene));
}

TEST(Scene, RejectsInvalidEnvironmentLight) {
  Scene scene = make_default_scene();

  scene.environment.color = {-0.1f, 1.0f, 1.0f};
  EXPECT_FALSE(is_valid_scene(scene));

  scene = make_default_scene();
  scene.environment.intensity = -0.1f;
  EXPECT_FALSE(is_valid_scene(scene));
}

} // namespace
} // namespace raypalette