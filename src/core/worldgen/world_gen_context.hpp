#pragma once

#include <string>
#include <vector>
#include <map>
#include <glm/glm.hpp>

namespace deepbound
{

// Key/Value structure for Splines
struct terrain_spline_t
{
  std::vector<float> keys;   // Y positions (0.0 to 1.0)
  std::vector<float> values; // Thresholds

  auto evaluate(float t) const -> float
  {
    // Find interval
    if (keys.empty())
      return 0.0f;
    if (t <= keys.front())
      return values.front();
    if (t >= keys.back())
      return values.back();

    for (size_t i = 0; i < keys.size() - 1; ++i)
    {
      if (t >= keys[i] && t <= keys[i + 1])
      {
        float range = keys[i + 1] - keys[i];
        float local_t = (t - keys[i]) / range;
        return values[i] * (1.0f - local_t) + values[i + 1] * local_t; // Lerp
      }
    }
    return values.back();
  }
};

struct landform_variant_t
{
  std::string code;
  std::string hexcolor;
  float weight = 1.0f;
  bool use_climate = false;
  float min_temp = -50.0f;
  float max_temp = 50.0f;
  float min_rain = 0.0f;
  float max_rain = 255.0f;

  // Noise settings
  std::vector<double> terrain_octaves;
  terrain_spline_t y_key_thresholds;
};

struct rock_strata_variant_t
{
  std::string block_code;
  std::string rock_group; // Igneous, Sedimentary, Metamorphic, Volcanic
  // VS Logic: Amplitudes/Thresholds/Freqs for noise Layers
  std::vector<float> amplitudes;
  std::vector<float> thresholds;
  std::vector<float> frequencies;
  std::string gen_dir; // TopDown / BottomUp
};

struct geologic_province_variant_t
{
  std::string code;
  // map RockGroup -> MaxThickness
  std::map<std::string, float> rock_strata_thickness;
  float weight;
};

class world_gen_context_t
{
public:
  std::vector<landform_variant_t> landforms;
  std::vector<rock_strata_variant_t> rock_strata;
  std::vector<geologic_province_variant_t> geologic_provinces;

  // Helper lookups
  auto get_landform(const std::string &code) -> const landform_variant_t *;
};

} // namespace deepbound
