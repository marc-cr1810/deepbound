#pragma once

#include "core/worldgen/chunk.hpp"
#include "core/worldgen/fastnoise_wrapper.hpp"
#include "core/worldgen/world_gen_context.hpp"
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <map>
#include <mutex>

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
  float surface_noise = 0.0f;
  float upheaval = 0.0f;
  std::vector<double> blended_octaves;
  std::vector<double> blended_thresholds;
  float max_noise_amp = 0.0f;
};

class world_generator_t
{
public:
  world_generator_t();

  auto generate_chunk(int chunk_x, int chunk_y) -> std::unique_ptr<chunk_t>;

  static const resource_id_t AIR_ID;
  static const resource_id_t WATER_ID;

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

  // Reusable buffers for per-column generation
  struct strata_range_t
  {
    std::string code;
    int y_min, y_max;
  };

  struct cached_column_info_t
  {
    column_data_t data;
    int surface_y;
    std::vector<strata_range_t> strata_ranges;
    std::string last_bottom_up_code;
  };

  // Sharded cache to reduce mutex contention (32 shards)
  struct column_cache_shard_t
  {
    std::unordered_map<int, std::shared_ptr<cached_column_info_t>> map;
    std::mutex mutex;
  };
  std::array<column_cache_shard_t, 32> m_column_caches;

  // Reusable buffers for per-column generation

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
