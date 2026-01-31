#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>


namespace deepbound {

/**
 * @brief Represents a landform definition (terrain shape).
 * Based on Vintage Story's landforms.json
 */
struct landform_definition_t {
  std::string code;      // e.g. "rollinghills"
  float weight = 1.0f;   // Probability weight
  std::string hex_color; // Debug/Map color definition

  // Noise composition (Octaves)
  // Usually 9 values in VS, mapping to specific noise octaves
  std::vector<float> terrain_octaves;
  std::vector<float> terrain_octave_thresholds;

  // Terrain Curve (Spline/Lerp control points)
  // Defines the shape of the terrain at different noise values
  std::vector<float> terrain_y_key_positions;
  std::vector<float> terrain_y_key_thresholds;

  // Climate usage
  bool use_climate_map = false;
  float min_temp = -999.0f;
  float max_temp = 999.0f;
  float min_rain = -999.0f;
  float max_rain = 999.0f;
};

/**
 * @brief Registry for all landform definitions.
 */
class landform_registry_t {
public:
  static auto get() -> landform_registry_t &;

  landform_registry_t(const landform_registry_t &) = delete;
  auto operator=(const landform_registry_t &) -> landform_registry_t & = delete;

  auto register_landform(const landform_definition_t &definition) -> void;
  auto get_landform(const std::string &code) const
      -> const landform_definition_t *;
  auto get_all_landforms() const
      -> const std::map<std::string, landform_definition_t> &;

private:
  landform_registry_t() = default;
  std::map<std::string, landform_definition_t> m_landform_map;
};

} // namespace deepbound
