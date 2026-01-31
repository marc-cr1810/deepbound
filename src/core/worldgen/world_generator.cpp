#include "core/worldgen/world_generator.hpp"
#include "core/assets/json_loader.hpp"
#include "core/common/resource_id.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <map>

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

  std::string assets_path = "assets/worldgen";
  json_loader_t::load_worldgen(assets_path, m_context);

  std::cout << "WorldGen Initialized. Loaded " << m_context.landforms.size() << " landforms, " << m_context.rock_strata.size() << " strata, " << m_context.geologic_provinces.size() << " provinces." << std::endl;
  m_initialized = true;
}

auto world_generator_t::get_landform(float x, float y, float temp, float rain) -> const landform_variant_t *
{
  if (m_context.landforms.empty())
    return nullptr;

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

  float noise_pick = (m_noise.get_noise(x * 0.0002f, 13) + 1.0f) * 0.5f;
  float r = noise_pick * total_weight;

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

  float surface_noise = m_noise.get_terrain_noise(x, 0, lf->terrain_octaves);
  return get_density_fast(x, y, surface_noise, lf);
}

auto world_generator_t::get_density_fast(float x, float y, float surface_noise, const landform_variant_t *lf) -> float
{
  if (!lf)
    return -1.0f;

  float normalized_y = y / (float)m_world_height;
  float y_offset = lf->y_key_thresholds.evaluate(normalized_y);

  float base_density = (y_offset - 0.5f) * 6.0f + surface_noise;
  float upheaval = m_noise.get_noise(x * 0.0005f, 555) * 0.1f;
  base_density += upheaval;

  float detail_noise = m_noise.get_terrain_noise(x, y * 0.25f, {0, 0, 0.05, 0.05, 0.02});

  return base_density + detail_noise;
}

auto world_generator_t::get_province(float x, float y) -> const geologic_province_variant_t *
{
  if (m_context.geologic_provinces.empty())
    return nullptr;

  float noise_val = m_noise.get_noise(x * 0.001f, 99);
  float normalized = (noise_val + 1.0f) * 0.5f;
  size_t index = (size_t)(normalized * m_context.geologic_provinces.size());
  if (index >= m_context.geologic_provinces.size())
    index = m_context.geologic_provinces.size() - 1;

  return &m_context.geologic_provinces[index];
}

auto world_generator_t::get_rock_strata(float x, float y, float density, const geologic_province_variant_t *province) -> std::string
{
  return "deepbound:rock-granite";
}

auto world_generator_t::generate_chunk(int chunk_x, int chunk_y) -> std::unique_ptr<chunk_t>
{
  auto chunk = std::make_unique<chunk_t>(chunk_x, chunk_y);

  int world_x_base = chunk_x * CHUNK_SIZE;
  int world_y_base = chunk_y * CHUNK_SIZE;

  int prev_surface_y = m_world_height / 2;

  for (int x = 0; x < CHUNK_SIZE; ++x)
  {
    int wx = world_x_base + x;

    float sample_x = (float)wx;
    float temp = m_temp_noise.get_noise(sample_x * 0.0001f, 0) * 50.0f;
    float rain = (m_rain_noise.get_noise(sample_x * 0.0001f, 0) + 1.0f) * 128.0f;

    const landform_variant_t *current_lf = get_landform(sample_x, 0, temp, rain);

    // Safety check for null landform
    if (!current_lf)
    {
      // Fallback or skip
      // For now, if we have NO landforms, we can't do much.
      // Assume a default or return early to avoid crash.
      // However, we need to fill the chunk with SOMETHING.
      // Let's just create a dummy/default if logic allows, or just skip advanced logic.
    }

    const geologic_province_variant_t *current_province = get_province(sample_x, 0);

    // 2. Optimized Surface Search
    int surface_y = 0;
    // THREAD SAFETY: Surface cache uses atomics or mutex if global, but here we just clear it periodically or it's per-generator
    // For now, let's just re-calculate if not in cache (mutex removed for speed)
    auto cache_it = m_surface_cache.find(wx);
    if (cache_it != m_surface_cache.end())
    {
      surface_y = cache_it->second;
    }
    else
    {
      float h_noise = 0.0f;
      float y_offset = 0.5f;

      if (current_lf)
      {
        h_noise = m_noise.get_terrain_noise((float)wx, 0, current_lf->terrain_octaves);
      }

      float upheaval = m_noise.get_noise((float)wx * 0.0005f, 555) * 0.1f;

      int start_y = std::min(m_world_height - 1, std::max(0, prev_surface_y + 32));
      for (int sy = start_y; sy >= 0; --sy)
      {
        float normalized_y = (float)sy / (float)m_world_height;
        if (current_lf)
        {
          y_offset = current_lf->y_key_thresholds.evaluate(normalized_y);
        }

        if ((y_offset - 0.5f) * 6.0f + h_noise + upheaval > 0.0f)
        {
          surface_y = sy;
          break;
        }
      }
      m_surface_cache[wx] = surface_y;
    }
    prev_surface_y = surface_y;

    // 3. THREAD SAFE: Use local buffer for column ranges
    struct local_strata_range_t
    {
      std::string code;
      int y_min, y_max;
    };
    std::vector<local_strata_range_t> local_ranges;
    std::map<std::string, float> rock_group_usage;

    int ylower = 0;
    int yupper = surface_y;

    for (const auto &stratum : m_context.rock_strata)
    {
      std::vector<float> scaled_freqs = stratum.frequencies;
      for (auto &f : scaled_freqs)
        f *= 0.1f; // Broad layers

      float thickness_raw = m_noise.get_custom_noise((float)wx, 0.0f, stratum.amplitudes, stratum.thresholds, scaled_freqs);

      // Base thickness of 20 ensures we ALWAYS see layers, random variance on top
      float thickness = thickness_raw * 10.0f + 20.0f;

      float max_allowed = 999.0f;
      if (current_province)
      {
        auto it = current_province->rock_strata_thickness.find(stratum.rock_group);
        if (it != current_province->rock_strata_thickness.end())
        {
          max_allowed = it->second * 2.0f; // Scale for world height
        }
      }

      float allowed = max_allowed - rock_group_usage[stratum.rock_group];
      float actual_thickness = std::min(thickness, std::max(0.0f, allowed));

      if (actual_thickness >= 2.0f) // Minimum visible thickness
      {
        if (stratum.gen_dir == "TopDown")
        {
          local_ranges.push_back({"deepbound:" + stratum.block_code, (int)(yupper - actual_thickness), yupper});
          yupper -= (int)actual_thickness;
        }
        else
        {
          local_ranges.push_back({"deepbound:" + stratum.block_code, ylower, (int)(ylower + actual_thickness)});
          ylower += (int)actual_thickness;
        }
        rock_group_usage[stratum.rock_group] += actual_thickness;
      }
    }

    // 4. Column block filling
    float h_noise = 0.0f;
    if (current_lf)
      h_noise = m_noise.get_terrain_noise((float)wx, 0, current_lf->terrain_octaves);
    float upheaval = m_noise.get_noise((float)wx * 0.0005f, 555) * 0.1f;

    for (int y = 0; y < CHUNK_SIZE; ++y)
    {
      int wy = world_y_base + y;

      if (wy >= m_world_height)
      {
        chunk->set_tile(x, y, {"deepbound:air"});
        continue;
      }

      float density = get_density_fast((float)wx, (float)wy, h_noise, current_lf);

      if (density > 0.0f)
      {
        // Default fallback to obsidian for EASY verification (Purple/Black)
        // If the world is purple/black, we know strata logic is matching NONE.
        std::string rock_type = "deepbound:rock-obsidian";

        for (const auto &range : local_ranges)
        {
          float b_noise = m_noise.get_noise((float)wx * 0.02f, (float)wy * 0.02f) * 2.0f;
          if (wy >= (float)range.y_min + b_noise && wy <= (float)range.y_max + b_noise)
          {
            rock_type = range.code;
            break;
          }
        }
        chunk->set_tile(x, y, {rock_type});
      }
      else
      {
        if (wy < m_sea_level)
          chunk->set_tile(x, y, {"deepbound:water"});
        else
          chunk->set_tile(x, y, {"deepbound:air"});
      }
    }
  }

  return chunk;
}

} // namespace deepbound
