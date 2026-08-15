#pragma once

#include "math/color.hpp"

#include <cstdint>

namespace raypalette {

enum class MaterialType : std::uint32_t {
  Surface,
  Dielectric,
  Emissive,
};

enum class MaterialPreset : std::uint32_t {
  Custom,
  Matte,
  Glossy,
  Metal,
  Cloth,
  Skin,
  Hair,
};

struct MaterialCapabilities {
  bool metallic = false;
  bool specular = false;
  bool sheen = false;
  bool subsurface = false;
  bool anisotropy = false;
  bool coat = false;
  bool coat_roughness = false;
};

RAYPALETTE_HOST_DEVICE inline MaterialCapabilities material_capabilities(MaterialPreset preset) {
  switch (preset) {
  case MaterialPreset::Custom:
    return {true, true, true, true, true, true, true};
  case MaterialPreset::Glossy:
    return {false, true, false, false, false, true, true};
  case MaterialPreset::Metal:
    return {true, false, false, false, false, false, false};
  case MaterialPreset::Cloth:
    return {false, true, true, false, false, false, false};
  case MaterialPreset::Skin:
    return {false, true, false, true, false, false, false};
  case MaterialPreset::Hair:
    return {false, true, false, false, true, false, false};
  case MaterialPreset::Matte:
    return {false, true, false, false, false, false, false};
  }
  return {};
}

struct Material {
  MaterialType type = MaterialType::Surface;
  MaterialPreset preset = MaterialPreset::Custom;
  Vec3 base_color{0.8f, 0.8f, 0.8f};
  float metallic = 0.0f;
  float roughness = 0.5f;
  float specular = 0.5f;
  float index_of_refraction = 1.5f;
  Vec3 emission_color{};
  float emission_strength = 0.0f;
  Vec3 transmission_color{1.0f, 1.0f, 1.0f};
  float absorption_density = 0.0f;
  float coat = 0.0f;
  float coat_roughness = 0.03f;
  float sheen = 0.0f;
  float subsurface = 0.0f;
  float anisotropy = 0.0f;
};

struct PrincipledParameters {
  Vec3 base_color;
  float metallic;
  float roughness;
  float specular;
  float transmission;
  float index_of_refraction;
  Vec3 transmission_color;
  float absorption_density;
  float coat;
  float coat_roughness;
  float sheen;
  float subsurface;
  float anisotropy;
  Vec3 emission_color;
  float emission_strength;
};

inline void apply_material_preset(Material& material, MaterialPreset preset) {
  material.type = MaterialType::Surface;
  material.preset = preset;
  switch (material.preset) {
  case MaterialPreset::Matte:
    material.metallic = 0.0f;
    material.roughness = 0.85f;
    material.coat = 0.0f;
    material.coat_roughness = 0.0f;
    material.sheen = 0.0f;
    material.subsurface = 0.0f;
    material.anisotropy = 0.0f;
    break;
  case MaterialPreset::Glossy:
    material.metallic = 0.0f;
    material.roughness = 0.22f;
    material.coat = 0.7f;
    material.coat_roughness = 0.04f;
    material.sheen = 0.0f;
    material.subsurface = 0.0f;
    material.anisotropy = 0.0f;
    break;
  case MaterialPreset::Metal:
    material.metallic = 1.0f;
    material.coat = 0.0f;
    material.coat_roughness = 0.0f;
    material.sheen = 0.0f;
    material.subsurface = 0.0f;
    material.anisotropy = 0.0f;
    break;
  case MaterialPreset::Cloth:
    material.metallic = 0.0f;
    material.roughness = 0.8f;
    material.coat = 0.0f;
    material.coat_roughness = 0.0f;
    material.sheen = 0.6f;
    material.subsurface = 0.0f;
    material.anisotropy = 0.0f;
    break;
  case MaterialPreset::Skin:
    material.metallic = 0.0f;
    material.roughness = 0.45f;
    material.coat = 0.0f;
    material.coat_roughness = 0.0f;
    material.sheen = 0.0f;
    material.subsurface = 0.35f;
    material.anisotropy = 0.0f;
    break;
  case MaterialPreset::Hair:
    material.metallic = 0.0f;
    material.roughness = 0.35f;
    material.coat = 0.0f;
    material.coat_roughness = 0.0f;
    material.sheen = 0.0f;
    material.subsurface = 0.0f;
    material.anisotropy = 0.8f;
    break;
  case MaterialPreset::Custom:
    break;
  }
}

RAYPALETTE_HOST_DEVICE inline PrincipledParameters
resolve_principled_parameters(const Material& material) {
  return {material.base_color,
          material.metallic,
          material.roughness,
          material.specular,
          0.0f,
          material.index_of_refraction,
          material.transmission_color,
          material.absorption_density,
          material.coat,
          material.coat_roughness,
          material.sheen,
          material.subsurface,
          material.anisotropy,
          material.emission_color,
          material.emission_strength};
}

RAYPALETTE_HOST_DEVICE inline bool is_valid_material(const Material& material) {
  if (!is_unit_color(material.base_color) || !is_unit_color(material.emission_color) ||
      !is_unit_color(material.transmission_color) || material.roughness < 0.0f ||
      material.roughness > 1.0f || material.metallic < 0.0f || material.metallic > 1.0f ||
      material.specular < 0.0f || material.specular > 1.0f || material.emission_strength < 0.0f ||
      material.absorption_density < 0.0f || material.coat < 0.0f || material.coat > 1.0f ||
      material.coat_roughness < 0.0f || material.coat_roughness > 1.0f || material.sheen < 0.0f ||
      material.sheen > 1.0f || material.subsurface < 0.0f || material.subsurface > 1.0f ||
      material.anisotropy < -1.0f || material.anisotropy > 1.0f) {
    return false;
  }

  return material.type != MaterialType::Dielectric || material.index_of_refraction > 1.0f;
}

} // namespace raypalette
