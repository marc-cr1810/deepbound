#pragma once

#include "core/worldgen/chunk.hpp"
#include "core/worldgen/fastnoise_wrapper.hpp"
#include "core/worldgen/world_gen_context.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <map>

namespace deepbound
{

struct landform_weight_t
{
  const landform_variant_t *landform;
  float weight;
};

struct column_data_t
{
  std::vector<landform_weight_t> weights;
  float surface_noise;
  float upheaval;
};

class world_generator_t
{
public:
  world_generator_t();

  auto generate_chunk(int chunk_x, int chunk_y) -> std::unique_ptr<chunk_t>;

private:
  auto init_context() -> void;
  // returns density -1 to 1
  auto get_density(float x, float y, float z) -> float;
  // Optimized column density helpers
  auto prepare_column_data(float x, float z) -> column_data_t;
  auto get_density_from_column(float x, float y, const column_data_t &data) -> float;

  auto get_landform(float x, float y) -> const landform_variant_t *;
  auto get_landform_weights(float x, float y, std::vector<landform_weight_t> &out_weights) -> void;

  // New: Geologic Province
  auto get_province(float x, float y) -> const geologic_province_variant_t *;

  // New: Rock Strata
  // Returns block code (e.g., "deepbound:rock-granite")
  auto get_rock_strata(float x, float y, float density, const geologic_province_variant_t *province) -> std::string;

  world_gen_context_t m_context;
  fast_noise_wrapper_t m_noise;

  int m_world_height = 512;
  int m_sea_level = 220; // Default VS logic (110/256 * 512)

  // Cache for surface heights to avoid redundant searches
  std::unordered_map<int, int> m_surface_cache;

  // Reusable buffers for per-column generation to avoid allocations
  struct strata_range_t
  {
    std::string code;
    int y_min, y_max;
  };
  std::vector<strata_range_t> m_column_ranges_buffer;
  std::map<std::string, float> m_rock_group_cur_buffer;

  // Climate noise
  fast_noise_wrapper_t m_temp_noise;
  fast_noise_wrapper_t m_rain_noise;

  // Province noise
  fast_noise_wrapper_t m_province_noise;

  // Strata noise (reused for strata layers)
  fast_noise_wrapper_t m_strata_noise;

  // Upheaval noise
  fast_noise_wrapper_t m_upheaval_noise;

  bool m_initialized = false;
};

} // namespace deepbound
