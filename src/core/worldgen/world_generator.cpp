#include "core/worldgen/world_generator.hpp"
#include "core/assets/json_loader.hpp"
#include "core/common/resource_id.hpp"
#include <iostream>
#include <cmath>

namespace deepbound
{

world_generator_t::world_generator_t() : m_noise(1337), m_temp_noise(123), m_rain_noise(456), m_province_noise(789), m_strata_noise(4242), m_upheaval_noise(999)
{
  init_context();
}

auto world_generator_t::init_context() -> void
{
  if (m_initialized)
    return;

  // Hardcoded path to reference assets for now - In production, use configurated asset path
  // Assuming we are running from project root/build
  // Target "assets/worldgen" as requested by user
  std::string assets_path = "c:/Programming/C++/deepbound/assets/worldgen";
  json_loader_t::load_worldgen(assets_path, m_context);

  std::cout << "WorldGen Initialized. Loaded " << m_context.landforms.size() << " landforms." << std::endl;
  m_initialized = true;
}

auto world_generator_t::get_landform(float x, float y, float temp, float rain) -> const landform_variant_t *
{
  if (m_context.landforms.empty())
    return nullptr;

  // Weighted choice among candidates.
  float total_weight = 0.0f;
  std::vector<const landform_variant_t *> candidates;

  for (const auto &lf : m_context.landforms)
  {
    if (lf.use_climate)
    {
      if (temp >= lf.min_temp && temp <= lf.max_temp && rain >= lf.min_rain && rain <= lf.max_rain)
      {
        candidates.push_back(&lf);
        total_weight += lf.weight;
      }
    }
    else
    {
      candidates.push_back(&lf);
      total_weight += lf.weight;
    }
  }

  if (candidates.empty())
    return &m_context.landforms[0];

  // Pick deterministic weighted random based on coordinates
  // Use a simple hash of x, y to pick the landform
  uint32_t seed = (uint32_t)x * 73856093 ^ (uint32_t)y * 19349663;
  float r = (float)(seed % 1000) / 1000.0f * total_weight;

  float current_weight = 0.0f;
  for (const auto *lf : candidates)
  {
    current_weight += lf->weight;
    if (r <= current_weight)
      return lf;
  }

  return candidates.back();
}

auto world_generator_t::get_density(float x, float y, const landform_variant_t *lf) -> float
{
  if (!lf)
    return -1.0f;

  // 1. Base Height Noise (Horizontal only)
  // This defines the rolling hills / surface shape.
  float surface_noise = m_noise.get_terrain_noise(x, 0, lf->terrain_octaves);

  // 2. Y-Gradient (The "offset")
  float map_height = 1024.0f;
  float normalized_y = y / map_height;
  float y_offset = lf->y_key_thresholds.evaluate(normalized_y);

  // 3. Combine into base density
  // Normalize threshold to -1..1 range if needed, but assuming 0..1 from JSON
  // If y_offset is 1.0 at bottom and 0.0 at top, we want it to cross 0 at the surface.
  // Surface is where surface_noise + (y_offset - 0.5) == 0
  float base_density = (y_offset - 0.5f) * 4.0f + surface_noise;

  // 4. Detail Noise (2D) for overhangs and interest (low frequency)
  float detail_noise = m_noise.get_terrain_noise(x, y * 0.5f, {0, 0, 0.1, 0.2, 0.1});

  return base_density + detail_noise;
}

auto world_generator_t::get_province(float x, float y) -> const geologic_province_variant_t *
{
  if (m_context.geologic_provinces.empty())
    return nullptr;

  float noise_val = m_province_noise.get_cellular(x, y, 0.002f); // Low freq for large provinces

  // Map -1..1 to 0..size-1
  float normalized = (noise_val + 1.0f) * 0.5f; // 0..1
  size_t index = (size_t)(normalized * m_context.geologic_provinces.size());
  if (index >= m_context.geologic_provinces.size())
    index = m_context.geologic_provinces.size() - 1;

  return &m_context.geologic_provinces[index];
}

auto world_generator_t::get_rock_strata(float x, float y, float density, const geologic_province_variant_t *province) -> std::string
{
  // Default string
  std::string selected_rock = "deepbound:rock-granite";

  bool province_has_strata = (province != nullptr && !province->rock_strata_thickness.empty());

  for (const auto &strata : m_context.rock_strata)
  {
    // Check Province constraints
    if (province_has_strata)
    {
      // Check if this strata's rock group is allowed in this province
      auto it = province->rock_strata_thickness.find(strata.rock_group);
      if (it == province->rock_strata_thickness.end() || it->second <= 0.0f)
      {
        continue;
      }
    }

    float freq = strata.frequencies.empty() ? 0.05f : strata.frequencies[0];
    float threshold = strata.thresholds.empty() ? 0.5f : strata.thresholds[0];

    float noise_val = m_strata_noise.get_noise(x * freq, y * freq);

    if (strata.gen_dir == "TopDown")
    {
      if (y > 450 && noise_val > threshold)
      {
        return "deepbound:" + strata.block_code;
      }
    }
    else if (strata.gen_dir == "BottomUp")
    {
      if (y < 400 && noise_val > threshold)
      {
        return "deepbound:" + strata.block_code;
      }
    }
  }

  return selected_rock;
}

auto world_generator_t::generate_chunk(int chunk_x, int chunk_y) -> std::unique_ptr<Chunk>
{
  auto chunk = std::make_unique<Chunk>(chunk_x, chunk_y);

  int world_x_base = chunk_x * CHUNK_SIZE;
  int world_y_base = chunk_y * CHUNK_SIZE;

  const landform_variant_t *current_lf = nullptr;
  const geologic_province_variant_t *current_province = nullptr;

  for (int x = 0; x < CHUNK_SIZE; ++x)
  {
    int wx = world_x_base + x;

    // Smoothing: Sample climate and landform once every 32 blocks to minimize vertical artifacts.
    float sample_x = std::floor((float)wx / 32.0f) * 32.0f;
    float temp = m_temp_noise.get_noise(sample_x * 0.001f, 0) * 50.0f;
    float rain = (m_rain_noise.get_noise(sample_x * 0.001f, 0) + 1.0f) * 128.0f;

    current_lf = get_landform(sample_x, 0, temp, rain);
    current_province = get_province(sample_x, 0);

    for (int y = 0; y < CHUNK_SIZE; ++y)
    {
      int wy = world_y_base + y;

      float density = get_density((float)wx, (float)wy, current_lf);

      if (density > 0.0f)
      {
        // SOLID
        std::string rock_type = get_rock_strata((float)wx, (float)wy, density, current_province);
        chunk->set_tile(x, y, {rock_type});
      }
      else
      {
        // AIR or WATER (Sea Level at ~440 for 1024 height - approx 43%)
        int sea_level = 440;
        if (wy < sea_level)
        {
          chunk->set_tile(x, y, {"deepbound:water"});
        }
        else
        {
          chunk->set_tile(x, y, {"deepbound:air"});
        }
      }
    }
  }

  return chunk;
}

} // namespace deepbound
