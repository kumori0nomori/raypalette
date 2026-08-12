#pragma once

#include "math/color.hpp"

#include <cstdint>

namespace raypalette {

enum class MaterialType : std::uint32_t {
  Diffuse,
  Metal,
  Dielectric,
  Emissive,
};

struct Material {
  MaterialType type = MaterialType::Diffuse;
  Vec3 base_color{0.8f, 0.8f, 0.8f};
  float roughness = 0.5f;
  float index_of_refraction = 1.5f;
  Vec3 emission_color{};
  float emission_strength = 0.0f;
};

RAYPALETTE_HOST_DEVICE inline bool is_valid_material(const Material &material) {
  if (!is_unit_color(material.base_color) || !is_unit_color(material.emission_color) ||
      material.roughness < 0.0f || material.roughness > 1.0f ||
      material.emission_strength < 0.0f) {
    return false;
  }

  return material.type != MaterialType::Dielectric ||
         material.index_of_refraction >= 1.0f;
}

} // namespace raypalette
